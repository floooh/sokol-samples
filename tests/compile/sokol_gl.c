#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_IMPL
#include "sokol_gl.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sgl_setup(&(sgl_desc_t){0});
    return (sapp_desc){0};
}
