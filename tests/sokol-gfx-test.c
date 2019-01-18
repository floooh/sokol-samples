//------------------------------------------------------------------------------
//  sokol-gfx-test.c
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_DUMMY_BACKEND
#include "sokol_gfx.h"
#include "test.h"

static void test_init_shutdown(void) {
    test("sokol-gfx init/shutdown");
    sg_setup(&(sg_desc){0});
    T(sg_isvalid());
    sg_shutdown();
}

static void test_pool_size(void) {
    test("sokol-gfx pool size");
    sg_setup(&(sg_desc){
        .buffer_pool_size = 1024,
        .image_pool_size = 2048,
        .shader_pool_size = 128,
        .pipeline_pool_size = 256,
        .pass_pool_size = 64,
        .context_pool_size = 64
    });
    T(sg_isvalid());
    /* pool slot 0 is reserved (this is the "invalid slot") */
    T(_sg.pools.buffer_pool.size == 1025);
    T(_sg.pools.image_pool.size == 2049);
    T(_sg.pools.shader_pool.size == 129);
    T(_sg.pools.pipeline_pool.size == 257);
    T(_sg.pools.pass_pool.size == 65);
    T(_sg.pools.context_pool.size == 65);
    T(_sg.pools.buffer_pool.queue_top == 1024);
    T(_sg.pools.image_pool.queue_top == 2048);
    T(_sg.pools.shader_pool.queue_top == 128);
    T(_sg.pools.pipeline_pool.queue_top == 256);
    T(_sg.pools.pass_pool.queue_top == 64);
    T(_sg.pools.context_pool.queue_top == 63);  /* default context has been created already */
    sg_shutdown();
}

int main() {
    test_begin("sokol-gfx-test");
    test_init_shutdown();
    test_pool_size();
    return test_end();
}