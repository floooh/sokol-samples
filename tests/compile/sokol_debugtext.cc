#include "sokol_gfx.h"
#include "sokol_app.h"
#define SOKOL_IMPL
#include "sokol_debugtext.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sdtx_setup({ });
    return { };
}
