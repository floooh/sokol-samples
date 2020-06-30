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

void init() {
    // setup sokol_gfx and sokol_time
    sg_desc desc = {
        .context = osx_get_context()
    };
    sg_setup(&desc);
    stm_setup();
    simgui_desc_t simgui_desc = { };
    simgui_setup(&simgui_desc);

    // setup the imgui environment
    ImGuiIO& io = ImGui::GetIO();
    io.KeyMap[ImGuiKey_Tab] = 0x30;
    io.KeyMap[ImGuiKey_LeftArrow] = 0x7B;
    io.KeyMap[ImGuiKey_RightArrow] = 0x7C;
    io.KeyMap[ImGuiKey_DownArrow] = 0x7D;
    io.KeyMap[ImGuiKey_UpArrow] = 0x7E;
    io.KeyMap[ImGuiKey_Home] = 0x73;
    io.KeyMap[ImGuiKey_End] = 0x77;
    io.KeyMap[ImGuiKey_Delete] = 0x75;
    io.KeyMap[ImGuiKey_Backspace] = 0x33;
    io.KeyMap[ImGuiKey_Enter] = 0x24;
    io.KeyMap[ImGuiKey_Escape] = 0x35;
    io.KeyMap[ImGuiKey_A] = 0x00;
    io.KeyMap[ImGuiKey_C] = 0x08;
    io.KeyMap[ImGuiKey_V] = 0x09;
    io.KeyMap[ImGuiKey_X] = 0x07;
    io.KeyMap[ImGuiKey_Y] = 0x10;
    io.KeyMap[ImGuiKey_Z] = 0x06;

    // OSX => ImGui input forwarding
    osx_mouse_pos([] (float x, float y) { ImGui::GetIO().MousePos = ImVec2(x, y); });
    osx_mouse_btn_down([] (int btn)     { ImGui::GetIO().MouseDown[btn] = true; });
    osx_mouse_btn_up([] (int btn)       { ImGui::GetIO().MouseDown[btn] = false; });
    osx_mouse_wheel([] (float v)        { ImGui::GetIO().MouseWheel = 0.25f * v; });
    osx_key_down([] (int key)           { if (key < 512) ImGui::GetIO().KeysDown[key] = true; });
    osx_key_up([] (int key)             { if (key < 512) ImGui::GetIO().KeysDown[key] = false; });
    osx_char([] (wchar_t c)             { ImGui::GetIO().AddInputCharacter(c); });

    // initial clear color
    pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };
}

void frame() {
    const int width = osx_width();
    const int height = osx_height();
    simgui_new_frame(width, height, stm_sec(stm_laptime(&last_time)));

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &pass_action.colors[0].val[0]);
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
    sg_begin_default_pass(&pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    simgui_shutdown();
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, 1, "Sokol Dear ImGui (Metal)", init, frame, shutdown);
    return 0;
}
