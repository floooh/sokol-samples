#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_IMPL
#include "sokol_shape.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sshape_plane_sizes(10);
    return { };
}
