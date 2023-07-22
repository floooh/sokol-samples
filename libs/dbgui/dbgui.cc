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

static sg_imgui_t sg_imgui;

void __dbgui_setup(int sample_count) {
    // setup debug inspection header(s)
    const sg_imgui_desc_t desc = { };
    sg_imgui_init(&sg_imgui, &desc);

    // setup the sokol-imgui utility header
    simgui_desc_t simgui_desc = { };
    simgui_desc.sample_count = sample_count;
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);
}

void __dbgui_shutdown(void) {
    sg_imgui_discard(&sg_imgui);
    simgui_shutdown();
}

void __dbgui_draw(void) {
    simgui_new_frame({ sapp_width(), sapp_height(), sapp_frame_duration(), sapp_dpi_scale() });
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("sokol-gfx")) {
            ImGui::MenuItem("Capabilities", 0, &sg_imgui.caps.open);
            ImGui::MenuItem("Buffers", 0, &sg_imgui.buffers.open);
            ImGui::MenuItem("Images", 0, &sg_imgui.images.open);
            ImGui::MenuItem("Samplers", 0, &sg_imgui.samplers.open);
            ImGui::MenuItem("Shaders", 0, &sg_imgui.shaders.open);
            ImGui::MenuItem("Pipelines", 0, &sg_imgui.pipelines.open);
            ImGui::MenuItem("Passes", 0, &sg_imgui.passes.open);
            ImGui::MenuItem("Calls", 0, &sg_imgui.capture.open);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    sg_imgui_draw(&sg_imgui);
    simgui_render();
}

void __dbgui_event(const sapp_event* e) {
    simgui_handle_event(e);
}

bool __dbgui_event_with_retval(const sapp_event* e) {
    return simgui_handle_event(e);
}

} // extern "C"
