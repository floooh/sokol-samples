//------------------------------------------------------------------------------
//  instancing-compute-sapp.c
//
//  Same as instancing-sapp.c and instancing-pull-sapp.c, but compute the
//  particle positions in a compute shader.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "instancing-compute-sapp.glsl.h"

#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    int num_particles;
    float ry;
    struct {
        sg_buffer buf;
        sg_pipeline pip;
    } compute;
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
} state = {
    .display = {
        .pass_action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.0f, 0.2f, 0.1f, 1.0f }
            }
        }
    }
};

static vs_params_t compute_vsparams(float frame_time);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // create a zero-initialized storage buffer for the particle state
    state.compute.buf = sg_make_buffer(&(sg_buffer_desc){
        .size = MAX_PARTICLES * sizeof(particle_t),
        .usage.storage_buffer = true,
        .label = "particle-buffer",
    });

    // a compute shader and pipeline object for updating particle positions
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(update_shader_desc(sg_query_backend())),
        .label = "update-pipeline",
    });

    // vertex and index buffer for the particle geometry
    const float r = 0.05f;
    const float vertices[] = {
        // positions            colors
        0.0f,   -r, 0.0f,       1.0f, 0.0f, 0.0f, 1.0f,
           r, 0.0f, r,          0.0f, 1.0f, 0.0f, 1.0f,
           r, 0.0f, -r,         0.0f, 0.0f, 1.0f, 1.0f,
          -r, 0.0f, -r,         1.0f, 1.0f, 0.0f, 1.0f,
          -r, 0.0f, r,          0.0f, 1.0f, 1.0f, 1.0f,
        0.0f,    r, 0.0f,       1.0f, 0.0f, 1.0f, 1.0f
    };
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.display.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "geometry-vbuf",
    });
    state.display.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "geometry-ibuf",
    });

    // shader and pipeline for rendering the particles, this uses
    // the compute-updated storage buffer to provide the particle positions
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 }, // position
                [1] = { .format = SG_VERTEXFORMAT_FLOAT4 }, // color
            },
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
        .label = "render-pipeline",
    });

    // one-time init of particle velocities in a compute shader
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(init_shader_desc(sg_query_backend())),
    });
    sg_begin_pass(&(sg_pass){ .compute = true });
    sg_apply_pipeline(pip);
    sg_apply_bindings(&(sg_bindings){ .storage_buffers[SBUF_vs_ssbo] = state.compute.buf });
    sg_dispatch(MAX_PARTICLES / 64, 1, 1);
    sg_end_pass();
    sg_destroy_pipeline(pip);
}

static void frame(void) {
    state.num_particles += NUM_PARTICLES_EMITTED_PER_FRAME;
    if (state.num_particles > MAX_PARTICLES) {
        state.num_particles = MAX_PARTICLES;
    }
    const float dt = (float)sapp_frame_duration();

    // compute pass to update particle positions
    const cs_params_t cs_params = {
        .dt = dt,
        .num_particles = state.num_particles,
    };
    sg_begin_pass(&(sg_pass){ .compute = true, .label = "compute-pass" });
    sg_apply_pipeline(state.compute.pip);
    sg_apply_bindings(&(sg_bindings){
        .storage_buffers[SBUF_cs_ssbo] = state.compute.buf,
    });
    sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
    sg_dispatch((state.num_particles+63)/64, 1, 1);
    sg_end_pass();

    // render pass to render the particles via instancing, with the
    // instance positions coming from the storage buffer
    const vs_params_t vs_params = compute_vsparams(dt);
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain(),
        .label = "render-pass",
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.display.vbuf,
        .index_buffer = state.display.ibuf,
        .storage_buffers[SBUF_vs_ssbo] = state.compute.buf,
    });
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.num_particles);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

static vs_params_t compute_vsparams(float frame_time) {
    hmm_mat4 proj = HMM_Perspective(60.0f, sapp_widthf()/sapp_heightf(), 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.ry += 60.0f * frame_time;
    return (vs_params_t) {
        .mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f))),
    };
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "instancing-compute-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
