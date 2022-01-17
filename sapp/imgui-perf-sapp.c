//------------------------------------------------------------------------------
//  imguiperf-sapp.c
//  Test performance of the sokol_imgui.h rendering backend with many
//  windows.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#include <math.h> // sinf, cosf

static const int max_windows = 128;

static struct {
    uint64_t last_time;
    int num_windows;
    double min_raw_frame_time;
    double max_raw_frame_time;
    double min_rounded_frame_time;
    double max_rounded_frame_time;
    float counter;
    sg_pass_action pass_action;
} state = {
    .num_windows = 16,
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.5f, 0.7f, 1.0f } }
    }
};

static void reset_minmax_frametimes(void) {
    state.max_raw_frame_time = 0;
    state.min_raw_frame_time = 1000.0;
    state.max_rounded_frame_time = 0;
    state.min_rounded_frame_time = 1000.0;
}

static void init(void) {
    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
    });
    stm_setup();
    simgui_setup(&(simgui_desc_t){0});
    reset_minmax_frametimes();
}

static void frame(void) {
    const int width = sapp_width();
    const int height = sapp_height();
    const float fwidth = (float)width;
    const float fheight = (float)height;
    double raw_frame_time = stm_sec(stm_laptime(&state.last_time));
    double rounded_frame_time = sapp_frame_duration();
    if (raw_frame_time > 0) {
        if (raw_frame_time < state.min_raw_frame_time) {
            state.min_raw_frame_time = raw_frame_time;
        }
        if (raw_frame_time > state.max_raw_frame_time) {
            state.max_raw_frame_time = raw_frame_time;
        }
    }
    if (rounded_frame_time > 0) {
        if (rounded_frame_time < state.min_rounded_frame_time) {
            state.min_rounded_frame_time = rounded_frame_time;
        }
        if (rounded_frame_time > state.max_rounded_frame_time) {
            state.max_rounded_frame_time = rounded_frame_time;
        }
    }

    simgui_new_frame(&(simgui_frame_desc_t){
        .width = width,
        .height = height,
        .delta_time = rounded_frame_time,
        .dpi_scale = sapp_dpi_scale()
    });

    // controls window
    igSetNextWindowPos((ImVec2){ 10, 10 }, ImGuiCond_Once, (ImVec2){0,0});
    igSetNextWindowSize((ImVec2){ 500, 0 }, ImGuiCond_Once);
    igBegin("Controls", 0, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);
    igSliderInt("Num Windows", &state.num_windows, 1, max_windows, "%d", ImGuiSliderFlags_None);
    igText("raw frame time:     %.3fms (min: %.3f, max: %.3f)",
        raw_frame_time * 1000.0,
        state.min_raw_frame_time * 1000.0,
        state.max_raw_frame_time * 1000.0);
    igText("rounded frame time: %.3fms (min: %.3f, max: %.3f)",
        rounded_frame_time * 1000.0,
        state.min_rounded_frame_time * 1000.0,
        state.max_rounded_frame_time * 1000.0);
    if (igButton("Reset min/max times", (ImVec2){0,0})) {
        reset_minmax_frametimes();
    }
    igEnd();

    // test windows
    state.counter += 1.0f;
    for (int i = 0; i < state.num_windows; i++) {
        const float t = state.counter + i * 2.0f;
        const float r = (float)i / (float)max_windows;
        const float x = fwidth * (0.5f + (r * 0.5f * 0.75f * sinf(t * 0.05f)));
        const float y = fheight * (0.5f + (r * 0.5f * 0.75f * cosf(t * 0.05f)));
        char name[64];
        snprintf(name, sizeof(name), "Hello ImGui %d", i);
        igSetNextWindowPos((ImVec2){x,y}, ImGuiCond_Always, (ImVec2){0,0});
        igSetNextWindowSize((ImVec2){100, 10}, ImGuiCond_Always);
        igBegin(name, 0, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoFocusOnAppearing);
        igEnd();
    }

    // draw everything
    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "imgui perftest",
        .icon.sokol_default = true,
    };
}
