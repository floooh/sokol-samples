//------------------------------------------------------------------------------
//  cimgui-sapp.c
//
//  Demonstrates Dear ImGui UI rendering in C via
//  sokol_gfx.h + sokol_imgui.h + cimgui.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
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
        .gl = {
            .force_gles2 = sapp_gles2()
        },
        .mtl = {
            .device = sapp_metal_get_device(),
            .renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
            .drawable_cb = sapp_metal_get_drawable
        },
        .d3d11 = {
            .device = sapp_d3d11_get_device(),
            .device_context = sapp_d3d11_get_device_context(),
            .render_target_view_cb = sapp_d3d11_get_render_target_view,
            .depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view,
        },
        .wgpu = {
            .device = sapp_wgpu_get_device(),
            .render_format = sapp_wgpu_get_render_format(),
            .render_view_cb = sapp_wgpu_get_render_view,
            .resolve_view_cb = sapp_wgpu_get_resolve_view,
            .depth_stencil_view_cb = sapp_wgpu_get_depth_stencil_view
        }
    });
    stm_setup();

    // use sokol-imgui with all default-options (we're not doing
    // multi-sampled rendering or using non-default pixel formats)
    simgui_setup(&(simgui_desc_t){ 0 });

    /* initialize application state */
    state = (state_t) {
        .show_test_window = true,
        .pass_action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR, .val = { 0.7f, 0.5f, 0.0f, 1.0f }
            }
        }
    };
}

void frame(void) {
    const int width = sapp_width();
    const int height = sapp_height();
    const double delta_time = stm_sec(stm_laptime(&state.last_time));
    simgui_new_frame(width, height, delta_time);

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    igText("Hello, world!");
    igSliderFloat("float", &f, 0.0f, 1.0f, "%.3f", 1.0f);
    igColorEdit3("clear color", &state.pass_action.colors[0].val[0], 0);
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

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
    if (state.show_test_window) {
        igSetNextWindowPos((ImVec2){460,20}, ImGuiCond_FirstUseEver, (ImVec2){0,0});
        igShowDemoWindow(0);
    }

    // the sokol_gfx draw pass
    sg_begin_default_pass(&state.pass_action, width, height);
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
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 1024,
        .height = 768,
        .gl_force_gles2 = true,
        .window_title = "cimgui (sokol-app)",
        .ios_keyboard_resizes_canvas = false
    };
}
