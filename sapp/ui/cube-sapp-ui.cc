//------------------------------------------------------------------------------
//  cube-sapp-ui.cc
//  Optional debugging UI implementation for cube-sapp
//------------------------------------------------------------------------------
#include "sokol_gfx_imgui.h"
#define UI_IMPL
#include "ui.h"

extern "C" {

void dbgui_setup(void) {
    imgui_init();
}

void dbgui_shutdown(void) {

}

void dbgui_draw(void) {
    imgui_newframe();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("sokol-gfx")) {
            ImGui::MenuItem("Global State");
            ImGui::MenuItem("Buffers");
            ImGui::MenuItem("Images");
            ImGui::MenuItem("Shaders");
            ImGui::MenuItem("Pipelines");
            ImGui::MenuItem("Passes");
            ImGui::MenuItem("Call List");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    imgui_draw();
}

void dbgui_event(const sapp_event* e) {
    imgui_event(e);
}

} // extern "C"