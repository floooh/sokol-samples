//------------------------------------------------------------------------------
//  imgui-metal.c
//  Since this will only need to compile with clang we can
//  mix C99 designated initializers and C++ code.
//
//  NOTE: this demo is using the sokol_imgui.h utility header which
//  implements a renderer for Dear ImGui on top of sokol_gfx.h, but without
//  sokol_app.h (via the config define SOKOL_IMGUI_NO_SOKOL_APP).
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_log.h"
#include "imgui.h"
#define SOKOL_METAL
#define SOKOL_IMGUI_IMPL
#define SOKOL_IMGUI_NO_SOKOL_APP
#include "sokol_imgui.h"

static const int WIDTH = 1024;
static const int HEIGHT = 768;

static uint64_t last_time = 0;
static bool show_test_window = true;
static bool show_another_window = false;
static sg_pass_action pass_action;

static ImGuiKey as_imgui_key(int keycode);

void init() {
    // setup sokol_gfx and sokol_time
    const sg_desc desc = {
        .environment = osx_environment(),
        .logger = {
            .func = slog_func,
        }
    };
    sg_setup(&desc);
    stm_setup();
    const simgui_desc_t simgui_desc = {
        .logger.func = slog_func,
    };
    simgui_setup(&simgui_desc);

    // OSX => ImGui input forwarding
    osx_mouse_pos([] (float x, float y) { ImGui::GetIO().AddMousePosEvent(x, y); });
    osx_mouse_btn_down([] (int btn)     { ImGui::GetIO().AddMouseButtonEvent(btn, true); });
    osx_mouse_btn_up([] (int btn)       { ImGui::GetIO().AddMouseButtonEvent(btn, false); });
    osx_mouse_wheel([] (float v)        { ImGui::GetIO().AddMouseWheelEvent(0.0f, 0.25f * v); });
    osx_key_down([] (int key)           { ImGui::GetIO().AddKeyEvent(as_imgui_key(key), true); });
    osx_key_up([] (int key)             { ImGui::GetIO().AddKeyEvent(as_imgui_key(key), false); });
    osx_char([] (wchar_t c)             { ImGui::GetIO().AddInputCharacter(c); });

    // initial clear color
    pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };
}

void frame() {
    const int width = osx_width();
    const int height = osx_height();
    simgui_new_frame({width, height, stm_sec(stm_laptime(&last_time)), 1.0f });

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &pass_action.colors[0].clear_value.r);
    if (ImGui::Button("Test Window")) show_test_window ^= 1;
    if (ImGui::Button("Another Window")) show_another_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (show_another_window) {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiCond_FirstUseEver);
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello");
        ImGui::End();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowDemoWindow()
    if (show_test_window) {
        ImGui::SetNextWindowPos(ImVec2(460, 20), ImGuiCond_FirstUseEver);
        ImGui::ShowDemoWindow();
    }

    // the sokol draw pass
    sg_begin_pass({ .action = pass_action, .swapchain = osx_swapchain() });
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    simgui_shutdown();
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, 1, SG_PIXELFORMAT_NONE, "Sokol Dear ImGui (Metal)", init, frame, shutdown);
    return 0;
}

static ImGuiKey as_imgui_key(int keycode) {
    switch (keycode) {
        case 0x30: return ImGuiKey_Tab;
        case 0x7B: return ImGuiKey_LeftArrow;
        case 0x7C: return ImGuiKey_RightArrow;
        case 0x7D: return ImGuiKey_DownArrow;
        case 0x7E: return ImGuiKey_UpArrow;
        case 0x73: return ImGuiKey_Home;
        case 0x77: return ImGuiKey_End;
        case 0x75: return ImGuiKey_Delete;
        case 0x33: return ImGuiKey_Backspace;
        case 0x24: return ImGuiKey_Enter;
        case 0x35: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    }
}
