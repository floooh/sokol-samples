//------------------------------------------------------------------------------
//  sokol-fetch-test.c
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

UTEST(sokol_fetch, handle) {
    sfetch_handle_t h = _sfetch_make_handle(123, 456);
    T(h.id == ((456<<16)|123));
    T(_sfetch_handle_index(h) == 123);
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
    sfetch_handle_t h = _sfetch_make_handle(1, 1);
    _sfetch_item_init(&item, h, &request);
    T(item.handle.id == h.id);
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

UTEST(sokol_fetch, pool_init_discard) {
    _sfetch_pool_t pool = { };
    const int num_items = 127;
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
    _sfetch_pool_t pool = { };
    const int num_items = 16;
    _sfetch_pool_init(&pool, num_items);
    


    _sfetch_pool_discard(&pool);
}

/* public API functions */
UTEST(sokol_fetch, max_path) {
    T(sfetch_max_path() == SFETCH_MAX_PATH);
}

UTEST(sokol_fetch, max_userdata) {
    T(sfetch_max_userdata() == (SFETCH_MAX_USERDATA_UINT64 * sizeof(uint64_t)));
}

