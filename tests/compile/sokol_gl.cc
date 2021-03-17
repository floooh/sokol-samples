#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_IMPL
#include "sokol_gl.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sgl_setup({ });
    return { };
}
