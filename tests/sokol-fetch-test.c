//------------------------------------------------------------------------------
//  sokol-fetch-test.c
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#include "sokol_fetch.h"
#include "utest.h"

#define T(b) EXPECT_TRUE(b)
#define TSTR(s0, s1) EXPECT_TRUE(0 == strcmp(s0,s1))

// internal functions
UTEST(sokol_fetch, item_init_discard) {
    sfetch_request_t request = {
        .path = sfetch_make_path("hello_world.txt")
    };
    TSTR(request.path.buf, "hello_world.txt");
    _sfetch_item_t item = { };
    sfetch_handle_t h = _sfetch_make_handle(1, 1);
    _sfetch_item_init(&item, h, &request);
    T(item.handle.id == h.id);
    T(item.state == SFETCH_STATE_INITIAL);
    TSTR(item.request.path.buf, request.path.buf);
    item.state = SFETCH_STATE_OPENING;
    _sfetch_item_discard(&item);
    T(item.handle.id == 0);
    T(item.request.path.buf[0] == 0);
    T(item.state == SFETCH_STATE_INITIAL);
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
