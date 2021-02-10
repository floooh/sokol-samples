//------------------------------------------------------------------------------
//  imgui-wgpu.c
//
//  Test ImGui rendering, no input handling!
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#define SOKOL_IMGUI_NO_SOKOL_APP
#include "sokol_imgui.h"
#include "wgpu_entry.h"

static struct {
    sg_pass_action pass_action;
    uint64_t last_time = 0;
    bool show_test_window = true;
    bool show_another_window = false;
} state;


static void init(void) {
    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_desc desc = { };
    desc.context = wgpu_get_context();
    sg_setup(&desc);
    stm_setup();

    // use sokol-imgui with all default-options (we're not doing
    // multi-sampled rendering or using non-default pixel formats)
    simgui_desc_t simgui_desc = { };
    simgui_setup(&simgui_desc);

    // initial clear color
    state.pass_action.colors[0].action = SG_ACTION_CLEAR;
    state.pass_action.colors[0].value = { 0.0f, 0.5f, 0.7f, 1.0f };
}

static void frame(void) {
    const int width = wgpu_width();
    const int height = wgpu_height();
    const double delta_time = stm_sec(stm_laptime(&state.last_time));
    simgui_new_frame(width, height, delta_time);

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &state.pass_action.colors[0].value.r);
    if (ImGui::Button("Test Window")) state.show_test_window ^= 1;
    if (ImGui::Button("Another Window")) state.show_another_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (state.show_another_window) {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiCond_Once);
        ImGui::Begin("Another Window", &state.show_another_window);
        ImGui::Text("Hello");
        ImGui::End();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowDemoWindow()
    if (state.show_test_window) {
        ImGui::ShowDemoWindow();
    }

    // the sokol_gfx draw pass
    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    simgui_shutdown();
    sg_shutdown();
}

int main() {
    wgpu_desc_t desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.shutdown_cb = shutdown;
    desc.width = 1024;
    desc.height = 768;
    desc.title = "imgui-wgpu";
    wgpu_start(&desc);
    return 0;
}
