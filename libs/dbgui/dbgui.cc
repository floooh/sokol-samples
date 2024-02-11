//------------------------------------------------------------------------------
//  dbgui.cc
//  Implementation file for the generic debug UI overlay, using
//  the sokol_imgui.h utility header which implements the Dear ImGui
//  glue code.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"

extern "C" {

static sgimgui_t sgimgui;

void __dbgui_setup(int sample_count) {
    // setup debug inspection header(s)
    const sgimgui_desc_t desc = { };
    sgimgui_init(&sgimgui, &desc);

    // setup the sokol-imgui utility header
    simgui_desc_t simgui_desc = { };
    simgui_desc.sample_count = sample_count;
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);
}

void __dbgui_shutdown(void) {
    sgimgui_discard(&sgimgui);
    simgui_shutdown();
}

void __dbgui_draw(void) {
    simgui_new_frame({ sapp_width(), sapp_height(), sapp_frame_duration(), sapp_dpi_scale() });
    if (ImGui::BeginMainMenuBar()) {
        sgimgui_draw_menu(&sgimgui, "sokol-gfx");
        ImGui::EndMainMenuBar();
    }
    sgimgui_draw(&sgimgui);
    simgui_render();
}

void __dbgui_event(const sapp_event* e) {
    simgui_handle_event(e);
}

bool __dbgui_event_with_retval(const sapp_event* e) {
    return simgui_handle_event(e);
}

} // extern "C"
