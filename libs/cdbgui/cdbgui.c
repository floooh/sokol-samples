//------------------------------------------------------------------------------
//  cdbgui.c
//  Implementation file for the generic debug UI overlay, using the
//  sokol_imgui.h, sokol_gfx_cimgui.h headers and the C Dear ImGui bindings
//  cimgui.h
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#define SOKOL_APP_IMGUI_IMPL
#include "sokol_app_imgui.h"

void __cdbgui_setup(int sample_count) {
    // setup debug inspection headers
    sappimgui_setup();
    sgimgui_setup(&(sgimgui_desc_t){0});

    // setup the sokol-imgui utility header
    simgui_setup(&(simgui_desc_t){
        .sample_count = sample_count,
        .logger.func = slog_func,
    });
}

void __cdbgui_shutdown(void) {
    sgimgui_shutdown();
    sappimgui_shutdown();
    simgui_shutdown();
}

void __cdbgui_draw(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });
    sappimgui_track_frame();
    if (igBeginMainMenuBar()) {
        sappimgui_draw_menu("sokol-app");
        sgimgui_draw_menu("sokol-gfx");
        igEndMainMenuBar();
    }
    sappimgui_draw();
    sgimgui_draw();
    simgui_render();
}

void __cdbgui_event(const sapp_event* e) {
    sappimgui_track_event(e);
    simgui_handle_event(e);
}
