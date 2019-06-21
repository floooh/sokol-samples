//------------------------------------------------------------------------------
//  sokol-fetch-test.c
//
//  FIXME: simulate allocation errors
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SFETCH_MAX_USERDATA_UINT64 (8)
#define SFETCH_MAX_PATH (32)
#include "sokol_fetch.h"
#include "utest.h"

#define T(b) EXPECT_TRUE(b)
#define TSTR(s0, s1) EXPECT_TRUE(0 == strcmp(s0,s1))

typedef struct {
    int a, b, c;
} userdata_t;

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) {
    Sleep(ms);
}
#else
#include <unistd.h>
static void sleep_ms(int ms) {
    usleep(ms * 1000);
}
#endif

/* private implementation functions */
UTEST(sokol_fetch, path_make) {
    const char* str31 = "1234567890123456789012345678901";
    const char* str32 = "12345678901234567890123456789012";
    // max allowed string length (MAX_PATH - 1)
    _sfetch_path_t p31 = _sfetch_path_make(str31);
    TSTR(p31.buf, str31);
    // overflow
    _sfetch_path_t p32 = _sfetch_path_make(str32);
    T(p32.buf[0] == 0);
}

UTEST(sokol_fetch, make_id) {
    uint32_t slot_id = _sfetch_make_id(123, 456);
    T(slot_id == ((456<<16)|123));
    T(_sfetch_slot_index(slot_id) == 123);
}

UTEST(sokol_fetch, item_init_discard) {
    userdata_t user_data = {
        .a = 123,
        .b = 456,
        .c = 789
    };
    sfetch_request_t request = {
        .channel = 4,
        .path = "hello_world.txt",
        .user_data = &user_data,
        .user_data_size = sizeof(user_data)
    };
    _sfetch_item_t item = {0};
    uint32_t slot_id = _sfetch_make_id(1, 1);
    _sfetch_item_init(&item, slot_id, &request);
    T(item.handle.id == slot_id);
    T(item.channel == 4);
    T(item.lane == _SFETCH_INVALID_LANE);
    T(item.state == SFETCH_STATE_INITIAL);
    TSTR(item.path.buf, request.path);
    T(item.user.user_data_size == sizeof(userdata_t));
    const userdata_t* ud = (const userdata_t*) item.user.user_data;
    T((((uintptr_t)ud) & 0x7) == 0); // check alignment
    T(ud->a == 123);
    T(ud->b == 456);
    T(ud->c == 789);

    item.state = SFETCH_STATE_OPENING;
    _sfetch_item_discard(&item);
    T(item.handle.id == 0);
    T(item.path.buf[0] == 0);
    T(item.state == SFETCH_STATE_INITIAL);
    T(item.user.user_data_size == 0);
    T(item.user.user_data[0] == 0);
}

UTEST(sokol_fetch, item_init_path_overflow) {
    sfetch_request_t request = {
        .path = "012345678901234567890123456789012",
    };
    _sfetch_item_t item = {0};
    _sfetch_item_init(&item, _sfetch_make_id(1, 1), &request);
    T(item.path.buf[0] == 0);
}

UTEST(sokol_fetch, item_init_userdata_overflow) {
    uint8_t big_data[128] = { 0xFF };
    sfetch_request_t request = {
        .path = "hello_world.txt",
        .user_data = big_data,
        .user_data_size = sizeof(big_data)
    };
    _sfetch_item_t item = {0};
    _sfetch_item_init(&item, _sfetch_make_id(1, 1), &request);
    T(item.user.user_data_size == 0);
    T(item.user.user_data[0] == 0);
}

UTEST(sokol_fetch, pool_init_discard) {
    _sfetch_pool_t pool = {0};
    const uint32_t num_items = 127;
    T(_sfetch_pool_init(&pool, num_items));
    T(pool.valid);
    T(pool.size == 128);
    T(pool.free_top == 127);
    T(pool.free_slots[0] == 127);
    T(pool.free_slots[1] == 126);
    T(pool.free_slots[126] == 1);
    _sfetch_pool_discard(&pool);
    T(!pool.valid);
    T(pool.free_slots == 0);
    T(pool.items == 0);
}

UTEST(sokol_fetch, pool_alloc_free) {
    uint8_t buf[32];
    _sfetch_pool_t pool = {0};
    const uint32_t num_items = 16;
    _sfetch_pool_init(&pool, num_items);
    uint32_t slot_id = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){
        .path = "hello_world.txt",
        .buffer = {
            .ptr = buf,
            .size = sizeof(buf)
        },
    });
    T(slot_id == 0x00010001);
    T(pool.items[1].state == SFETCH_STATE_ALLOCATED);
    T(pool.items[1].handle.id == slot_id);
    TSTR(pool.items[1].path.buf, "hello_world.txt");
    T(pool.items[1].user.buffer.ptr == buf);
    T(pool.items[1].user.buffer.size == sizeof(buf));
    T(pool.free_top == 15);
    _sfetch_pool_item_free(&pool, slot_id);
    T(pool.items[1].handle.id == 0);
    T(pool.items[1].state == SFETCH_STATE_INITIAL);
    T(pool.items[1].path.buf[0] == 0);
    T(pool.items[1].user.buffer.ptr == 0);
    T(pool.items[1].user.buffer.size == 0);
    T(pool.free_top == 16);
    _sfetch_pool_discard(&pool);
}

UTEST(sokol_fetch, pool_overflow) {
    _sfetch_pool_t pool = {0};
    _sfetch_pool_init(&pool, 4);
    uint32_t id0 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path0" });
    uint32_t id1 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path1" });
    uint32_t id2 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path2" });
    uint32_t id3 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path3" });
    // next alloc should fail
    uint32_t id4 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path4" });
    T(id0 == 0x00010001);
    T(id1 == 0x00010002);
    T(id2 == 0x00010003);
    T(id3 == 0x00010004);
    T(id4 == 0);
    T(pool.items[1].handle.id == id0);
    T(pool.items[2].handle.id == id1);
    T(pool.items[3].handle.id == id2);
    T(pool.items[4].handle.id == id3);
    // free one item, alloc should work now
    _sfetch_pool_item_free(&pool, id0);
    uint32_t id5 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path5" });
    T(id5 == 0x00020001);
    T(pool.items[1].handle.id == id5);
    TSTR(pool.items[1].path.buf, "path5");
    _sfetch_pool_discard(&pool);
}

UTEST(sokol_fetch, lookup_item) {
    _sfetch_pool_t pool = {0};
    _sfetch_pool_init(&pool, 4);
    uint32_t id0 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path0" });
    uint32_t id1 = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){ .path="path1" });
    const _sfetch_item_t* item0 = _sfetch_pool_item_lookup(&pool, id0);
    const _sfetch_item_t* item1 = _sfetch_pool_item_lookup(&pool, id1);
    T(item0 == &pool.items[1]);
    T(item1 == &pool.items[2]);
    /* invalid handle always returns 0-ptr */
    T(0 == _sfetch_pool_item_lookup(&pool, _sfetch_make_id(0, 0)));
    /* free an item and make sure it's detected as dangling */
    _sfetch_pool_item_free(&pool, id0);
    T(0 == _sfetch_pool_item_lookup(&pool, id0));
    _sfetch_pool_discard(&pool);
}

UTEST(sokol_fetch, ring_init_discard) {
    _sfetch_ring_t ring = {0};
    const uint32_t num_slots = 4;
    T(_sfetch_ring_init(&ring, num_slots));
    T(ring.head == 0);
    T(ring.tail == 0);
    T(ring.num == (num_slots + 1));
    T(ring.buf);
    _sfetch_ring_discard(&ring);
    T(ring.head == 0);
    T(ring.tail == 0);
    T(ring.num == 0);
    T(ring.buf == 0);
}

UTEST(sokol_fetch, ring_enqueue_dequeue) {
    _sfetch_ring_t ring = {0};
    const uint32_t num_slots = 4;
    _sfetch_ring_init(&ring, num_slots);
    T(_sfetch_ring_count(&ring) == 0);
    T(_sfetch_ring_empty(&ring));
    T(!_sfetch_ring_full(&ring));
    for (uint32_t i = 0; i < num_slots; i++) {
        T(!_sfetch_ring_full(&ring));
        _sfetch_ring_enqueue(&ring, _sfetch_make_id(1, i+1));
        T(_sfetch_ring_count(&ring) == (i+1));
        T(!_sfetch_ring_empty(&ring));
    }
    T(_sfetch_ring_count(&ring) == 4);
    T(!_sfetch_ring_empty(&ring));
    T(_sfetch_ring_full(&ring));
    for (uint32_t i = 0; i < num_slots; i++) {
        T(_sfetch_ring_peek(&ring, i) == _sfetch_make_id(1, i+1));
    }
    for (uint32_t i = 0; i < num_slots; i++) {
        T(!_sfetch_ring_empty(&ring));
        const uint32_t slot_id = _sfetch_ring_dequeue(&ring);
        T(slot_id == _sfetch_make_id(1, i+1));
        T(!_sfetch_ring_full(&ring));
    }
    T(_sfetch_ring_count(&ring) == 0);
    T(_sfetch_ring_empty(&ring));
    T(!_sfetch_ring_full(&ring));
    _sfetch_ring_discard(&ring);
}

UTEST(sokol_fetch, ring_wrap_around) {
    _sfetch_ring_t ring = {0};
    _sfetch_ring_init(&ring, 4);
    uint32_t i = 0;
    for (i = 0; i < 4; i++) {
        _sfetch_ring_enqueue(&ring, _sfetch_make_id(1, i+1));
    }
    T(_sfetch_ring_full(&ring));
    for (; i < 64; i++) {
        T(_sfetch_ring_full(&ring));
        T(_sfetch_ring_dequeue(&ring) == _sfetch_make_id(1, (i - 3)));
        T(!_sfetch_ring_full(&ring));
        _sfetch_ring_enqueue(&ring, _sfetch_make_id(1, i+1));
    }
    T(_sfetch_ring_full(&ring));
    for (uint32_t i = 0; i < 4; i++) {
        T(_sfetch_ring_dequeue(&ring) == _sfetch_make_id(1, (i + 61)));
    }
    T(_sfetch_ring_empty(&ring));
    _sfetch_ring_discard(&ring);
}

UTEST(sokol_fetch, ring_wrap_count) {
    _sfetch_ring_t ring = {0};
    _sfetch_ring_init(&ring, 8);
    // add and remove 4 items to move tail to the middle
    for (uint32_t i = 0; i < 4; i++) {
        _sfetch_ring_enqueue(&ring, _sfetch_make_id(1, i+1));
        _sfetch_ring_dequeue(&ring);
        T(_sfetch_ring_empty(&ring));
    }
    // add another 8 items
    for (uint32_t i = 0; i < 8; i++) {
        _sfetch_ring_enqueue(&ring, _sfetch_make_id(1, i+1));
    }
    // now test, dequeue and test...
    T(_sfetch_ring_full(&ring));
    for (uint32_t i = 0; i < 8; i++) {
        T(_sfetch_ring_count(&ring) == (8 - i));
        _sfetch_ring_dequeue(&ring);
    }
    T(_sfetch_ring_count(&ring) == 0);
    T(_sfetch_ring_empty(&ring));
    _sfetch_ring_discard(&ring);
}

/* NOTE: channel_worker is called from a thread */
static int num_processed_items = 0;
static void channel_worker(_sfetch_t* ctx, uint32_t slot_id) {
    num_processed_items++;
}

UTEST(sokol_fetch, channel_init_discard) {
    num_processed_items = 0;
    _sfetch_channel_t chn = {0};
    const int num_slots = 12;
    const int num_lanes = 64;
    _sfetch_channel_init(&chn, 0, num_slots, num_lanes, channel_worker);
    T(chn.valid);
    T(_sfetch_ring_full(&chn.free_lanes));
    T(_sfetch_ring_empty(&chn.user_sent));
    T(_sfetch_ring_empty(&chn.user_incoming));
    T(_sfetch_ring_empty(&chn.thread_incoming));
    T(_sfetch_ring_empty(&chn.thread_outgoing));
    T(_sfetch_ring_empty(&chn.user_outgoing));
    _sfetch_channel_discard(&chn);
    T(!chn.valid);
}

/* public API functions */
UTEST(sokol_fetch, setup_shutdown) {
    sfetch_setup(&(sfetch_desc_t){0});
    T(sfetch_valid());
    // check default values
    T(sfetch_desc().max_requests == 128);
    T(sfetch_desc().num_channels == 1);
    T(sfetch_desc().num_lanes == 16);
    T(sfetch_desc().timeout_num_frames == 30);
    sfetch_shutdown();
    T(!sfetch_valid());
}

UTEST(sokol_fetch, setup_too_many_channels) {
    /* try to initialize with too many channels, this should clamp to
       SFETCH_MAX_CHANNELS
    */
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 64
    });
    T(sfetch_valid());
    T(sfetch_desc().num_channels == SFETCH_MAX_CHANNELS);
    sfetch_shutdown();
}

UTEST(sokol_fetch, max_path) {
    T(sfetch_max_path() == SFETCH_MAX_PATH);
}

UTEST(sokol_fetch, max_userdata) {
    T(sfetch_max_userdata_bytes() == (SFETCH_MAX_USERDATA_UINT64 * sizeof(uint64_t)));
}

static bool fail_open_passed;
static void fail_open_callback(sfetch_response_t response) {
    /* if opening a file fails, it will immediate switch into CLOSED state */
    if (response.state == SFETCH_STATE_FAILED) {
        fail_open_passed = true;
    }
}

UTEST(sokol_fetch, fail_open) {
    sfetch_setup(&(sfetch_desc_t){0});
    sfetch_handle_t h = sfetch_send(&(sfetch_request_t){
        .path = "non_existing_file.txt",
        .callback = fail_open_callback,
    });
    fail_open_passed = false;
    int frame_count = 0;
    while (sfetch_handle_valid(h) && (frame_count++ < 100)) {
        sfetch_dowork();
        sleep_ms(1);
    }
    T(fail_open_passed);
    sfetch_shutdown();
}

static bool load_file_fixed_buffer_passed;
static uint8_t load_file_buf[500000];
static const uint64_t combatsignal_file_size = 409482;

// The file callback is called from the "current user thread" (the same
// thread where the sfetch_send() for this request was called). Note that you
// can call sfetch_setup/shutdown() on multiple threads, each thread will
// get its own thread-local "sokol-fetch instance" and its own set of
// IO-channel threads.
static void load_file_fixed_buffer_callback(sfetch_response_t response) {
    // when loading the whole file at once, the COMPLETED state
    // is the best place to grab/process the data
    if (response.state == SFETCH_STATE_FETCHED) {
        if ((response.content_size == combatsignal_file_size) &&
            (response.chunk_offset == 0) &&
            (response.chunk.ptr == load_file_buf) &&
            (response.chunk.size == combatsignal_file_size) &&
            response.finished)
        {
            load_file_fixed_buffer_passed = true;
        }
    }
}

UTEST(sokol_fetch, load_file_fixed_buffer) {
    memset(load_file_buf, 0, sizeof(load_file_buf));
    sfetch_setup(&(sfetch_desc_t){0});
    // send a load-request for a file where we know the max size upfront,
    // so we can provide a buffer right in the fetch request (otherwise
    // the buffer needs to be provided in the callback when the request
    // is in OPENED state, since only then the file size will be known).
    sfetch_handle_t h = sfetch_send(&(sfetch_request_t){
        .path = "comsi.s3m",
        .callback = load_file_fixed_buffer_callback,
        .buffer = {
            .ptr = load_file_buf,
            .size = sizeof(load_file_buf)
        }
    });
    // simulate a frame-loop for as long as the request is in flight, normally
    // the sfetch_dowork() function is just called somewhere in the frame
    // to pump messages in and out of the IO threads, and invoke user-callbacks
    int frame_count = 0;
    while (sfetch_handle_valid(h) && (frame_count++ < 100)) {
        sfetch_dowork();
        sleep_ms(1);
    }
    T(load_file_fixed_buffer_passed);
    sfetch_shutdown();
}

/* tests whether files with unknown size are processed correctly */
static bool load_file_unknown_size_opened_passed;
static bool load_file_unknown_size_fetched_passed;
static void load_file_unknown_size_callback(sfetch_response_t response) {
    if (response.state == SFETCH_STATE_OPENED) {
        if ((response.content_size == combatsignal_file_size) &&
            (response.chunk_offset == 0) &&
            (response.chunk.ptr == 0) &&
            (response.chunk.size == 0) &&
            !response.finished)
        {
            load_file_unknown_size_opened_passed = true;
            sfetch_set_buffer(response.handle, &(sfetch_buffer_t){
                .ptr = load_file_buf,
                .size = sizeof(load_file_buf)
            });
        }
    }
    else if (response.state == SFETCH_STATE_FETCHED) {
        if ((response.content_size == combatsignal_file_size) &&
            (response.chunk_offset == 0) &&
            (response.chunk.ptr == load_file_buf) &&
            (response.chunk.size == combatsignal_file_size) &&
            response.finished)
        {
            load_file_unknown_size_fetched_passed = true;
        }
    }
}

UTEST(sokol_fetch, load_file_unknown_size) {
    memset(load_file_buf, 0, sizeof(load_file_buf));
    sfetch_setup(&(sfetch_desc_t){0});
    sfetch_handle_t h = sfetch_send(&(sfetch_request_t){
        .path = "comsi.s3m",
        .callback = load_file_unknown_size_callback
    });
    int frame_count = 0;
    while (sfetch_handle_valid(h) && (frame_count++ < 100)) {
        sfetch_dowork();
        sleep_ms(1);
    }
    T(load_file_unknown_size_opened_passed);
    T(load_file_unknown_size_fetched_passed);
    sfetch_shutdown();
}

/* tests whether not providing a buffer in SFETCH_STATE_OPENED properly fails */
static bool load_file_no_buffer_opened_passed;
static bool load_file_no_buffer_failed_passed;
static void load_file_no_buffer_callback(sfetch_response_t response) {
    if (response.state == SFETCH_STATE_OPENED) {
        if ((response.content_size == combatsignal_file_size) &&
            (response.chunk_offset == 0) &&
            (response.chunk.ptr == 0) &&
            (response.chunk.size == 0) &&
            !response.finished)
        {
            /* DO NOT provide a buffer here, see if that properly fails */
            load_file_no_buffer_opened_passed = true;
        }
    }
    else if (response.state == SFETCH_STATE_FAILED) {
        if (load_file_no_buffer_opened_passed) {
            load_file_no_buffer_failed_passed = true;
        }
    }
}

UTEST(sokol_fetch, load_file_no_buffer) {
    memset(load_file_buf, 0, sizeof(load_file_buf));
    sfetch_setup(&(sfetch_desc_t){0});
    sfetch_handle_t h = sfetch_send(&(sfetch_request_t){
        .path = "comsi.s3m",
        .callback = load_file_no_buffer_callback
    });
    int frame_count = 0;
    while (sfetch_handle_valid(h) && (frame_count++ < 100)) {
        sfetch_dowork();
        sleep_ms(1);
    }
    T(load_file_no_buffer_opened_passed);
    T(load_file_no_buffer_failed_passed);
    sfetch_shutdown();
}
