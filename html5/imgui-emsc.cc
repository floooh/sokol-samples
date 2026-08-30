//------------------------------------------------------------------------------
//  imgui-emsc
//  Demonstrates basic integration with Dear Imgui (without custom
//  texture or custom font support).
//  Since emscripten is using clang exclusively, we can use designated
//  initializers even though this is C++.
//------------------------------------------------------------------------------
#include "imgui.h"
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_log.h"
#include "emsc.h"
#define SOKOL_IMGUI_IMPL
#define SOKOL_IMGUI_NO_SOKOL_APP
#include "sokol_imgui.h"

// these are fairly recent warnings in clang
#pragma clang diagnostic ignored "-Wc99-designator"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wreorder-init-list"

static uint64_t last_time = 0;
static bool show_test_window = true;
static bool show_another_window = false;
static sg_pass_action pass_action;
static ImGuiKey as_imgui_key(int keycode);

static EM_BOOL draw(double time, void* userdata);

int main() {
    // setup WebGL context
    emsc_init("#canvas", EMSC_NONE);

    // setup sokol_gfx and sokol_time
    stm_setup();
    sg_setup(sg_desc{
        .environment = emsc_environment(),
        .logger = {
            .func = slog_func
        }
    });
    assert(sg_isvalid());
    simgui_setup(simgui_desc_t{});

    // emscripten to ImGui input forwarding
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddKeyEvent(as_imgui_key(e->keyCode), true);
            // only forward alpha-numeric keys to browser
            return e->keyCode < 32;
        });
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            if (e->keyCode < 512) {
                ImGui::GetIO().AddKeyEvent(as_imgui_key(e->keyCode), false);
            }
            // only forward alpha-numeric keys to browser
            return e->keyCode < 32;
        });
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddInputCharacter((ImWchar)e->charCode);
            return true;
        });
    emscripten_set_mousedown_callback("canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMouseButtonEvent(e->button, true);
            return true;
        });
    emscripten_set_mouseup_callback("canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMouseButtonEvent(e->button, false);
            return true;
        });
    emscripten_set_mousemove_callback("canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMousePosEvent((float)e->targetX, (float)e->targetY);
            return true;
        });
    emscripten_set_wheel_callback("canvas", nullptr, true,
        [](int, const EmscriptenWheelEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMouseWheelEvent(-0.1f * (float)e->deltaX, -0.1f * (float)e->deltaY);
            return true;
        });

    // initial clear color
    pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };

    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

// the main draw loop, this draw the standard ImGui demo windows
static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    simgui_new_frame(simgui_frame_desc_t{
        .width = emsc_width(),
        .height = emsc_height(),
        .delta_time = stm_sec(stm_laptime(&last_time)),
    });

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
    sg_begin_pass({ .action = pass_action, .swapchain = emsc_swapchain() });
    simgui_render();
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}

ImGuiKey as_imgui_key(int keycode) {
    switch (keycode) {
        case 9: return ImGuiKey_Tab;
        case 37: return ImGuiKey_LeftArrow;
        case 39: return ImGuiKey_RightArrow;
        case 38: return ImGuiKey_UpArrow;
        case 40: return ImGuiKey_DownArrow;
        case 36: return ImGuiKey_Home;
        case 35: return ImGuiKey_End;
        case 46: return ImGuiKey_Delete;
        case 8: return ImGuiKey_Backspace;
        case 13: return ImGuiKey_Enter;
        case 27: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    };
}
