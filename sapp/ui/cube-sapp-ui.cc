//------------------------------------------------------------------------------
//  cube-sapp-ui.cc
//  Optional debugging UI implementation for cube-sapp
//------------------------------------------------------------------------------
#include "sokol_gfx_imgui.h"
#define UI_IMPL
#include "ui.h"

extern "C" {

static sg_imgui_t sg_imgui;

void dbgui_setup(int sample_count) {
    imgui_init(sample_count);
    sg_imgui_init(&sg_imgui);
}

void dbgui_shutdown(void) {
    sg_imgui_discard(&sg_imgui);
}

void dbgui_draw(void) {
    imgui_newframe();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("sokol-gfx")) {
            ImGui::MenuItem("Global State");
            ImGui::MenuItem("Buffers", 0, &sg_imgui.buffers.open);
            ImGui::MenuItem("Images", 0, &sg_imgui.images.open);
            ImGui::MenuItem("Shaders", 0, &sg_imgui.shaders.open);
            ImGui::MenuItem("Pipelines", 0, &sg_imgui.pipelines.open);
            ImGui::MenuItem("Passes", 0, &sg_imgui.passes.open);
            ImGui::MenuItem("Call List");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    sg_imgui_draw(&sg_imgui);
    imgui_draw();
}

void dbgui_event(const sapp_event* e) {
    imgui_event(e);
}

} // extern "C"
