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
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#include "computeboids-sapp.glsl.h"

#define MAX_PARTICLES (10000)

static struct {
    sim_params_t sim_params;
    struct {
        sg_buffer buf[2];
        sg_view view[2];
        sg_pipeline pip;
    } compute;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
} state = {
    .sim_params = {
        .dt = 0.04f,
        .rule1_distance = 0.1f,
        .rule2_distance = 0.025f,
        .rule3_distance = 0.025f,
        .rule1_scale = 0.02f,
        .rule2_scale = 0.05f,
        .rule3_scale = 0.005f,
        .num_particles = 1500,
    },
    .display = {
        .pass_action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.0f, 0.15f, 0.3f, 1}
            },
        }
    }
};
static sgimgui_t sgimgui;

static void draw_ui(void);

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

// return a pseudo-random float between -1.0f and +1.0
static inline float rnd(void) {
    return (((float)(xorshift32() & 0xFFFF) / (float)0xFFFF) - 0.5f) * 2.0f;
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgimgui_init(&sgimgui, &(sgimgui_desc_t){0});
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });

    // two storage buffers and views with pre-initialized positions and velocities
    {
        const size_t initial_data_size = MAX_PARTICLES * sizeof(particle_t);
        particle_t* initial_data = calloc(1, initial_data_size);
        for (size_t i = 0; i < MAX_PARTICLES; i++) {
            initial_data[i] = (particle_t){
                .pos = { rnd(), rnd() },
                .vel = { rnd() * 0.1f, rnd() * 0.1f },
            };
        }
        for (size_t i = 0; i < 2; i++) {
            state.compute.buf[i] = sg_make_buffer(&(sg_buffer_desc){
                .usage.storage_buffer = true,
                .data = {
                    .ptr = initial_data,
                    .size = initial_data_size
                },
                .label = (i == 0) ? "particle-buffer-0" : "particle-buffer-1",
            });
            state.compute.view[i] = sg_make_view(&(sg_view_desc){
                .storage_buffer = { .buffer = state.compute.buf[i] },
                .label = (i == 0) ? "particle-view-0" : "particle-view-1",
            });
        }
        free(initial_data);
    }

    // compute shader and pipeline
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(compute_shader_desc(sg_query_backend())),
        .label = "compute-pipeline",
    });

    // a render pipeline and shader, note that vertices for the boids will be
    // synthesized by the vertex shader, so there's no separate vertex buffer,
    // we also don't need any non-default render state since the boids are just 2D triangles
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label = "render-pipeline",
    });
}

static void frame(void) {
    draw_ui();

    // input- and output- storage-buffers for this frame
    const sg_view in_view = state.compute.view[sapp_frame_count() & 1];
    const sg_view out_view = state.compute.view[(sapp_frame_count() + 1) & 1];

    // compute pass to update boid positions and velocities, this works with buffer-ping-ponging,
    // since the compute shader needs random access on the input parameters
    sg_begin_pass(&(sg_pass){ .compute = true, .label = "compute-pass" });
    sg_apply_pipeline(state.compute.pip);
    sg_apply_bindings(&(sg_bindings){
        .views = {
            [VIEW_cs_ssbo_in] = in_view,
            [VIEW_cs_ssbo_out] = out_view,
        },
    });
    sg_apply_uniforms(UB_sim_params, &SG_RANGE(state.sim_params));
    sg_dispatch((state.sim_params.num_particles+63)/64, 1, 1);
    sg_end_pass();

    // render pass for rendering the boids, instanced by the current output storage buffer
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_vs_ssbo] = out_view,
    });
    sg_draw(0, 3, state.sim_params.num_particles);
    simgui_render();
    sg_end_pass();
    sg_commit();

}

static void cleanup(void) {
    sgimgui_discard(&sgimgui);
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
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu(&sgimgui, "sokol-gfx");
        igEndMainMenuBar();
    }
    igSetNextWindowBgAlpha(0.8f);
    igSetNextWindowPos((ImVec2){10,30}, ImGuiCond_Once);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing;
    if (igBegin("controls", 0, flags)) {
        igSliderFloat("Delta T", &state.sim_params.dt, 0.01, 0.1);
        igSliderFloat("Rule1 Distance", &state.sim_params.rule1_distance, 0.0f, 0.2f);
        igSliderFloat("Rule2 Distance", &state.sim_params.rule2_distance, 0.0f, 0.1f);
        igSliderFloat("Rule3 Distance", &state.sim_params.rule3_distance, 0.0f, 0.1f);
        igSliderFloat("Rule1 Scale", &state.sim_params.rule1_scale, 0.0f, 0.1f);
        igSliderFloat("Rule2 Scale", &state.sim_params.rule2_scale, 0.0f, 0.1f);
        igSliderFloat("Rule3 Scale", &state.sim_params.rule3_scale, 0.0f, 0.1f);
        igSliderInt("Num Boids", &state.sim_params.num_particles, 0, MAX_PARTICLES);
    }
    igEnd();
    sgimgui_draw(&sgimgui);
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
