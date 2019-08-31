//------------------------------------------------------------------------------
//  cdbgui.c
//  Implementation file for the generic debug UI overlay, using the
//  sokol_imgui.h, sokol_gfx_cimgui.h headers and the C Dear ImGui bindings
//  cimgui.h
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"

static sg_imgui_t sg_imgui;

void __cdbgui_setup(int sample_count) {
    // setup debug inspection header(s)
    sg_imgui_init(&sg_imgui);

    // setup the sokol-imgui utility header
    simgui_setup(&(simgui_desc_t){
        .sample_count = sample_count
    });
}

void __cdbgui_shutdown(void) {
    simgui_shutdown();
    sg_imgui_discard(&sg_imgui);
}

void __cdbgui_draw(void) {
    simgui_new_frame(sapp_width(), sapp_height(), 1.0/60.0);
    if (igBeginMainMenuBar()) {
        if (igBeginMenu("sokol-gfx", true)) {
            igMenuItemBoolPtr("Buffers", 0, &sg_imgui.buffers.open, true);
            igMenuItemBoolPtr("Images", 0, &sg_imgui.images.open, true);
            igMenuItemBoolPtr("Shaders", 0, &sg_imgui.shaders.open, true);
            igMenuItemBoolPtr("Pipelines", 0, &sg_imgui.pipelines.open, true);
            igMenuItemBoolPtr("Passes", 0, &sg_imgui.passes.open, true);
            igMenuItemBoolPtr("Calls", 0, &sg_imgui.capture.open, true);
            igEndMenu();
        }
        igEndMainMenuBar();
    }
    sg_imgui_draw(&sg_imgui);
    simgui_render();
}

void __cdbgui_event(const sapp_event* e) {
    simgui_handle_event(e);
}
