//------------------------------------------------------------------------------
//  imgui-d3d11.cc
//  Dear ImGui integration sample with D3D11 backend.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_time.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#define SOKOL_IMGUI_NO_SOKOL_APP
#include "sokol_imgui.h"

static const int Width = 1024;
static const int Height = 768;

static uint64_t last_time = 0;
static bool show_test_window = true;
static bool show_another_window = false;
static sg_pass_action pass_action;

static ImGuiKey as_imgui_key(int keycode);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d11 app wrapper, sokol_gfx, sokol_time
    d3d11_desc_t d3d11_desc = {};
    d3d11_desc.width = Width;
    d3d11_desc.height = Height;
    d3d11_desc.title = L"imgui-d3d11.c";
    d3d11_init(&d3d11_desc);
    sg_desc desc = {};
    desc.environment = d3d11_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);
    stm_setup();
    simgui_desc_t simgui_desc = {};
    simgui_setup(&simgui_desc);

    // input forwarding
    d3d11_mouse_pos([] (float x, float y)   { ImGui::GetIO().AddMousePosEvent(x, y); });
    d3d11_mouse_btn_down([] (int btn)       { ImGui::GetIO().AddMouseButtonEvent(btn, true); });
    d3d11_mouse_btn_up([] (int btn)         { ImGui::GetIO().AddMouseButtonEvent(btn, false); });
    d3d11_mouse_wheel([](float v)           { ImGui::GetIO().AddMouseWheelEvent(0, v); });
    d3d11_char([] (wchar_t c)               { ImGui::GetIO().AddInputCharacter(c); });
    d3d11_key_down([] (int key)             { ImGui::GetIO().AddKeyEvent(as_imgui_key(key), true); });
    d3d11_key_up([] (int key)               { ImGui::GetIO().AddKeyEvent(as_imgui_key(key), false); });

    // initial clear color
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = { 0.0f, 0.5f, 0.7f, 1.0f };

    // draw loop
    while (d3d11_process_events()) {
        const int cur_width = d3d11_width();
        const int cur_height = d3d11_height();

        simgui_frame_desc_t simgui_frame_desc = {};
        simgui_frame_desc.width = cur_width;
        simgui_frame_desc.height = cur_height;
        simgui_frame_desc.delta_time = stm_sec(stm_laptime(&last_time));
        simgui_new_frame(&simgui_frame_desc);

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

        // the sokol_gfx draw pass
        sg_pass pass = { };
        pass.action = pass_action;
        pass.swapchain = d3d11_swapchain();
        sg_begin_pass(&pass);
        simgui_render();
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    simgui_shutdown();
    sg_shutdown();
    d3d11_shutdown();
}

static ImGuiKey as_imgui_key(int keycode) {
    switch (keycode) {
        case VK_TAB: return ImGuiKey_Tab;
        case VK_LEFT: return ImGuiKey_LeftArrow;
        case VK_RIGHT: return ImGuiKey_RightArrow;
        case VK_UP: return ImGuiKey_UpArrow;
        case VK_DOWN: return ImGuiKey_DownArrow;
        case VK_HOME: return ImGuiKey_Home;
        case VK_END: return ImGuiKey_End;
        case VK_DELETE: return ImGuiKey_Delete;
        case VK_BACK: return ImGuiKey_Backspace;
        case VK_RETURN: return ImGuiKey_Enter;
        case VK_ESCAPE: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    }
}
