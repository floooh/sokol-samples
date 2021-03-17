#include "sokol_app.h"
#include "sokol_gfx.h"
#include "imgui.h"
#define SOKOL_IMPL
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sg_imgui_init({ });
    return { };
}


