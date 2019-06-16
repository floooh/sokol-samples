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
        .path = "hello_world.txt",
        .user_data = &user_data,
        .user_data_size = sizeof(user_data)
    };
    _sfetch_item_t item = { };
    uint32_t slot_id = _sfetch_make_id(1, 1);
    _sfetch_item_init(&item, slot_id, &request);
    T(item.handle.id == slot_id);
    T(item.state == SFETCH_STATE_INITIAL);
    TSTR(item.path.buf, request.path);
    T(item.user_data_size == sizeof(userdata_t));
    const userdata_t* ud = (const userdata_t*) item.user_data;
    T((((uintptr_t)ud) & 0x7) == 0); // check alignment
    T(ud->a == 123);
    T(ud->b == 456);
    T(ud->c == 789);

    item.state = SFETCH_STATE_OPENING;
    _sfetch_item_discard(&item);
    T(item.handle.id == 0);
    T(item.path.buf[0] == 0);
    T(item.state == SFETCH_STATE_INITIAL);
    T(item.user_data_size == 0);
    T(item.user_data[0] == 0);
}

UTEST(sokol_fetch, item_init_path_overflow) {
    sfetch_request_t request = {
        .path = "012345678901234567890123456789012",
    };
    _sfetch_item_t item = { };
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
    _sfetch_item_t item = { };
    _sfetch_item_init(&item, _sfetch_make_id(1, 1), &request);
    T(item.user_data_size == 0);
    T(item.user_data[0] == 0);
}

UTEST(sokol_fetch, pool_init_discard) {
    _sfetch_pool_t pool = { };
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
    _sfetch_pool_t pool = { };
    const uint32_t num_items = 16;
    _sfetch_pool_init(&pool, num_items);
    uint32_t slot_id = _sfetch_pool_item_alloc(&pool, &(sfetch_request_t){
        .path = "hello_world.txt",
        .buffer = {
            .ptr = buf,
            .num_bytes = sizeof(buf)
        },
    });
    T(slot_id == 0x00010001);
    T(pool.items[1].state == SFETCH_STATE_ALLOCATED);
    T(pool.items[1].handle.id == slot_id);
    TSTR(pool.items[1].path.buf, "hello_world.txt");
    T(pool.items[1].buffer.ptr == buf);
    T(pool.items[1].buffer.num_bytes == sizeof(buf));
    T(pool.free_top == 15);
    _sfetch_pool_item_free(&pool, slot_id);
    T(pool.items[1].handle.id == 0);
    T(pool.items[1].state == SFETCH_STATE_INITIAL);
    T(pool.items[1].path.buf[0] == 0);
    T(pool.items[1].buffer.ptr == 0);
    T(pool.items[1].buffer.num_bytes == 0);
    T(pool.free_top == 16);
    _sfetch_pool_discard(&pool);
}

UTEST(sokol_fetch, pool_overflow) {
    _sfetch_pool_t pool = { };
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
    _sfetch_pool_t pool = { };
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
    _sfetch_ring_t ring = { };
    const uint32_t num_slots = 4;
    T(_sfetch_ring_init(&ring, num_slots));
    T(ring.head == 0);
    T(ring.tail == 0);
    T(ring.num == (num_slots + 1));
    T(ring.queue);
    _sfetch_ring_discard(&ring);
    T(ring.head == 0);
    T(ring.tail == 0);
    T(ring.num == 0);
    T(ring.queue == 0);
}

UTEST(sokol_fetch, ring_enqueue_dequeue) {
    _sfetch_ring_t ring = { };
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
    _sfetch_ring_t ring = { };
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

UTEST(sokol_fetch, queue_init_discard) {
    _sfetch_queue_t queue = { };
    const int num_slots = 12;
    _sfetch_queue_init(&queue, num_slots);
    T(queue.valid);
    T(queue.incoming.head == 0);
    T(queue.incoming.tail == 0);
    T(queue.incoming.num == (num_slots+1));
    T(queue.incoming.queue != 0);
    T(queue.outgoing.head == 0);
    T(queue.outgoing.tail == 0);
    T(queue.outgoing.num == (num_slots+1));
    T(queue.outgoing.queue != 0);
    _sfetch_queue_discard(&queue);
    T(!queue.valid);
    T(queue.incoming.head == 0);
    T(queue.incoming.tail == 0);
    T(queue.incoming.num == 0);
    T(queue.incoming.queue == 0);
    T(queue.outgoing.head == 0);
    T(queue.outgoing.tail == 0);
    T(queue.outgoing.num == 0);
    T(queue.outgoing.queue == 0);
}

UTEST(sokol_fetch, queue_enqueue_dequeue_incoming) {
    _sfetch_queue_t queue = { };
    _sfetch_queue_init(&queue, 8);
    _sfetch_ring_t src = { };
    _sfetch_ring_init(&src, 4);
    for (uint32_t i = 0; i < 4; i++) {
        _sfetch_ring_enqueue(&src, _sfetch_make_id(1, i+1));
    }
    T(_sfetch_ring_full(&src));
    _sfetch_queue_enqueue_incoming(&queue, &src);
    T(_sfetch_ring_empty(&src));
    T(_sfetch_ring_count(&queue.incoming) == 4);
    for (uint32_t i = 4; i < 8; i++) {
        _sfetch_ring_enqueue(&src, _sfetch_make_id(1, i+1));
    }
    T(_sfetch_ring_full(&src));
    _sfetch_queue_enqueue_incoming(&queue, &src);
    T(_sfetch_ring_empty(&src));
    T(_sfetch_ring_full(&queue.incoming));
    for (uint32_t i = 0; i < 8; i++) {
        T(_sfetch_queue_dequeue_incoming(&queue) == _sfetch_make_id(1, i+1));
    }
    T(_sfetch_ring_empty(&queue.incoming));
    T(_sfetch_queue_dequeue_incoming(&queue) == _sfetch_make_id(0, 0));
    _sfetch_ring_discard(&src);
    _sfetch_queue_discard(&queue);
}

UTEST(sokol_fetch, queue_enqueue_dequeue_outgoing) {
    _sfetch_queue_t queue = { };
    _sfetch_queue_init(&queue, 4);
    _sfetch_ring_t dst = { };
    _sfetch_ring_init(&dst, 8);
    for (uint32_t i = 0; i < 4; i++) {
        _sfetch_queue_enqueue_outgoing(&queue, _sfetch_make_id(1, i+1));
    }
    T(_sfetch_ring_full(&queue.outgoing));
    _sfetch_queue_dequeue_outgoing(&queue, &dst);
    T(_sfetch_ring_empty(&queue.outgoing));
    T(_sfetch_ring_count(&dst) == 4);
    for (uint32_t i = 4; i < 8; i++) {
        _sfetch_queue_enqueue_outgoing(&queue, _sfetch_make_id(1, i+1));
    }
    T(_sfetch_ring_full(&queue.outgoing));
    _sfetch_queue_dequeue_outgoing(&queue, &dst);
    T(_sfetch_ring_empty(&queue.outgoing));
    T(_sfetch_ring_full(&dst));
    _sfetch_ring_discard(&dst);
    _sfetch_queue_discard(&queue);
}

/* public API functions */
UTEST(sokol_fetch, max_path) {
    T(sfetch_max_path() == SFETCH_MAX_PATH);
}

UTEST(sokol_fetch, max_userdata) {
    T(sfetch_max_userdata_bytes() == (SFETCH_MAX_USERDATA_UINT64 * sizeof(uint64_t)));
}

