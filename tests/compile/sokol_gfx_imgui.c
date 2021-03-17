#include "sokol_app.h"
#include "sokol_gfx.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#if defined(_MSC_VER )
#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable:4214) // nonstandard extension used: bit field types other than int
#endif
#include "cimgui/cimgui.h"
#define SOKOL_IMPL
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sg_imgui_init(&(sg_imgui_t){0});
    return (sapp_desc){0};
}


