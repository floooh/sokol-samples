//------------------------------------------------------------------------------
//  sokol-gl-test.c
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_DUMMY_BACKEND
#include "sokol_gfx.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "test.h"

static void test_default_init_shutdown(void) {
    test("default init/shutdown");
    sg_setup(&(sg_desc){0});
    sgl_setup(&(sgl_desc_t){0});
    sgl_shutdown();
    sg_shutdown();
}

int main() {
    test_begin("sokol-gl-test");
    test_default_init_shutdown();
    return test_end();
}