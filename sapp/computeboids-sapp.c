//------------------------------------------------------------------------------
//  computeboids-sapp.c
//
//  A port of the WebGPU compute-boids sample
//  (https://webgpu.github.io/webgpu-samples/?sample=computeBoids)
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#include "computeboids-sapp.glsl.h"

// keep this in sync with the same constant in the GLSL
#define NUM_PARTICLES (1500)

static struct {
    sg_buffer particle_buffer[2];
    struct {
        sg_pass_action pass_action;
    } display;
} state = {
    .display = {
        .pass_action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1} },
        }
    }
};

static void draw_ui(void);

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

// return a random float value between -1.0f and +1.0
static inline float rnd(void) {
    return (((float)(xorshift32() & 0xFFFF) / (float)0xFFFF) - 0.5f) * 2.0f;
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    if (!sg_query_features().compute) {
        // compute not supported: early out
        return;
    }

    {
        const size_t initial_data_size = NUM_PARTICLES * sizeof(particle_t);
        particle_t* initial_data = calloc(1, initial_data_size);
        for (size_t i = 0; i < NUM_PARTICLES; i++) {
            initial_data[i] = (particle_t){
                .pos = { rnd(), rnd() },
                .vel = { rnd() * 0.1f, rnd() * 0.1f },
            };
        }
        for (size_t i = 0; i < 2; i++) {
            state.particle_buffer[i] = sg_make_buffer(&(sg_buffer_desc){
                .type = SG_BUFFERTYPE_STORAGEBUFFER,
                .data = {
                    .ptr = initial_data,
                    .size = initial_data_size
                },
            });
        }
        free(initial_data);
    }
}

static void frame(void) {
    draw_ui();
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain(),
    });
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

static void draw_ui(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    igSetNextWindowPos((ImVec2){20,20}, ImGuiCond_Once);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing;
    if (igBegin("controls", 0, flags)) {
        if (!sg_query_features().compute) {
            igText("Compute not supported!");
        } else {
            igText("F I X M E !!!");
        }
    }
    igEnd();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "computeboids-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
