//------------------------------------------------------------------------------
//  cdbgui.c
//  Implementation file for the generic debug UI overlay, using the
//  sokol_imgui.h, sokol_gfx_cimgui.h headers and the C Dear ImGui bindings
//  cimgui.h
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"

static sgimgui_t sg_imgui;

void __cdbgui_setup(int sample_count) {
    // setup debug inspection header(s)
    sgimgui_init(&sg_imgui, &(sgimgui_desc_t){0});

    // setup the sokol-imgui utility header
    simgui_setup(&(simgui_desc_t){
        .sample_count = sample_count,
        .logger.func = slog_func,
    });
}

void __cdbgui_shutdown(void) {
    sgimgui_discard(&sg_imgui);
    simgui_shutdown();
}

void __cdbgui_draw(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu(&sg_imgui, "sokol-gfx");
        igEndMainMenuBar();
    }
    sgimgui_draw(&sg_imgui);
    simgui_render();
}

void __cdbgui_event(const sapp_event* e) {
    simgui_handle_event(e);
}
