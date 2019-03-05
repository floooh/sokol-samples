//------------------------------------------------------------------------------
//  dbgui.cc
//  Implementation file for the generic debug UI overlay.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_gfx_imgui.h"
#define UI_IMPL
#include "ui.h"

extern "C" {

static sg_imgui_t sg_imgui;

void __dbgui_setup(int sample_count) {
    sg_imgui_init(&sg_imgui);
    imgui_init(sample_count);
}

void __dbgui_shutdown(void) {
    sg_imgui_discard(&sg_imgui);
}

void __dbgui_draw(void) {
    imgui_newframe();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("sokol-gfx")) {
            ImGui::MenuItem("Buffers", 0, &sg_imgui.buffers.open);
            ImGui::MenuItem("Images", 0, &sg_imgui.images.open);
            ImGui::MenuItem("Shaders", 0, &sg_imgui.shaders.open);
            ImGui::MenuItem("Pipelines", 0, &sg_imgui.pipelines.open);
            ImGui::MenuItem("Passes", 0, &sg_imgui.passes.open);
            ImGui::MenuItem("Calls", 0, &sg_imgui.capture.open);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    sg_imgui_draw(&sg_imgui);
    imgui_draw();
}

void __dbgui_event(const sapp_event* e) {
    imgui_event(e);
}

} // extern "C"
