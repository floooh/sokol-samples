//------------------------------------------------------------------------------
//  cdbgui.cc
//  Implementation file for the generic debug UI overlay, using the
//  C sokol_cimgui.h, sokol_gfx_cimgui.h and the C Dear ImGui bindings
//  cimgui.h
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_CIMGUI_IMPL
#include "sokol_cimgui.h"
#define SOKOL_GFX_CIMGUI_IMPL
#include "sokol_gfx_cimgui.h"

static sg_cimgui_t sg_imgui;

void __cdbgui_setup(int sample_count) {
    // setup debug inspection header(s)
    sg_cimgui_init(&sg_imgui);

    // setup the sokol-imgui utility header
    scimgui_setup(&(scimgui_desc_t){
        .sample_count = sample_count
    });
}

void __cdbgui_shutdown(void) {
    scimgui_shutdown();
    sg_cimgui_discard(&sg_imgui);
}

void __cdbgui_draw(void) {
    scimgui_new_frame(sapp_width(), sapp_height(), 1.0/60.0);
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
    sg_cimgui_draw(&sg_imgui);
    scimgui_render();
}

void __cdbgui_event(const sapp_event* e) {
    scimgui_handle_event(e);
}
