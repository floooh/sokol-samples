//------------------------------------------------------------------------------
//  cimgui-sapp.c
//
//  Demonstrates Dear ImGui UI rendering in C via
//  sokol_gfx.h + sokol_imgui.h + cimgui.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

typedef struct {
    uint64_t last_time;
    bool show_test_window;
    bool show_another_window;
    sg_pass_action pass_action;
} state_t;
static state_t state;

void init(void) {
    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    // use sokol-imgui with all default-options (we're not doing
    // multi-sampled rendering or using non-default pixel formats)
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });

    /* initialize application state */
    state = (state_t) {
        .show_test_window = true,
        .pass_action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.7f, 0.5f, 0.0f, 1.0f }
            }
        }
    };
}

void frame(void) {
    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = width,
        .height = height,
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    igText("Hello, world!");
    igSliderFloat("float", &f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_None);
    igColorEdit3("clear color", (float*)&state.pass_action.colors[0].clear_value, 0);
    if (igButton("Test Window", (ImVec2) { 0.0f, 0.0f})) state.show_test_window ^= 1;
    if (igButton("Another Window", (ImVec2) { 0.0f, 0.0f })) state.show_another_window ^= 1;
    igText("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / igGetIO()->Framerate, igGetIO()->Framerate);

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (state.show_another_window) {
        igSetNextWindowSize((ImVec2){200,100}, ImGuiCond_FirstUseEver);
        igBegin("Another Window", &state.show_another_window, 0);
        igText("Hello");
        igEnd();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowDemoWindow()
    if (state.show_test_window) {
        igSetNextWindowPos((ImVec2){460,20}, ImGuiCond_FirstUseEver, (ImVec2){0,0});
        igShowDemoWindow(0);
    }

    // the sokol_gfx draw pass
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

void input(const sapp_event* event) {
    simgui_handle_event(event);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 1024,
        .height = 768,
        .window_title = "cimgui (sokol-app)",
        .ios_keyboard_resizes_canvas = false,
        .icon.sokol_default = true,
        .enable_clipboard = true,
        .logger.func = slog_func,
    };
}
