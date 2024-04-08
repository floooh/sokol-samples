//------------------------------------------------------------------------------
//  instancing-pull-sapp.c
//
//  Same as instancing-sapp.c, but pull both vertex and instance data
//  from storage buffers.
//------------------------------------------------------------------------------
#include <stdlib.h> // rand()
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "instancing-pull-sapp.glsl.h"

#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float ry;
    int cur_num_particles;
    sb_instance_t inst[MAX_PARTICLES];
    hmm_vec3 vel[MAX_PARTICLES];
} state;

static void emit_particles(void);
static void update_particles(float frame_time);
static vs_params_t compute_vsparams(float frame_time);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // a pass action for the default render pass
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.1f, 0.2f, 1.0f } }
    };

    // a storage buffer for the static geometry
    const float r = 0.05f;
    const sb_vertex_t vertices[] = {
        { .pos = { 0.0f,   -r, 0.0f }, .color = { 1.0f, 0.0f, 0.0f, 1.0f } },
        { .pos = {    r, 0.0f, r    }, .color = { 0.0f, 1.0f, 0.0f, 1.0f } },
        { .pos = {    r, 0.0f, -r   }, .color = { 0.0f, 0.0f, 1.0f, 1.0f } },
        { .pos = {   -r, 0.0f, -r   }, .color = { 1.0f, 1.0f, 0.0f, 1.0f } },
        { .pos = {   -r, 0.0f, r    }, .color = { 0.0f, 1.0f, 1.0f, 1.0f } },
        { .pos = { 0.0f,    r, 0.0f }, .color = { 1.0f, 0.0f, 1.0f, 1.0f } },
    };
    state.bind.vs.storage_buffers[SLOT_vertices] = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_STORAGEBUFFER,
        .data = SG_RANGE(vertices),
        .label = "geometry-vertices",
    });

    // an index buffer for the static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "geometry-indices"
    });

    // a dynamic storage buffer for the per-instance data
    state.bind.vs.storage_buffers[SLOT_instances] = sg_make_buffer(&(sg_buffer_desc) {
        .type = SG_BUFFERTYPE_STORAGEBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = MAX_PARTICLES * sizeof(sb_instance_t),
        .label = "instance-data",
    });

    // a shader and pipeline object, note the lack of a vertex layout definition
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(instancing_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "instancing-pipeline",
    });
}

static void frame(void) {
    const float frame_time = (float)sapp_frame_duration();

    // emit new particles, and update particle positions
    emit_particles();
    update_particles(frame_time);

    // update instance data storage buffer
    sg_update_buffer(state.bind.vs.storage_buffers[SLOT_instances], &(sg_range){
        .ptr = state.inst,
        .size = (size_t)state.cur_num_particles * sizeof(sb_instance_t),
    });

    // compute model-view-projection matrix
    const vs_params_t vs_params = compute_vsparams(frame_time);

    // ...and draw
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.cur_num_particles);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

static void emit_particles(void) {
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (state.cur_num_particles < MAX_PARTICLES) {
            state.inst[state.cur_num_particles].pos = HMM_Vec3(0.0, 0.0, 0.0);
            state.vel[state.cur_num_particles] = HMM_Vec3(
                ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f,
                ((float)(rand() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f,
                ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f);
            state.cur_num_particles++;
        } else {
            break;
        }
    }
}

static void update_particles(float frame_time) {
    for (int i = 0; i < state.cur_num_particles; i++) {
        state.vel[i].Y -= 1.0f * frame_time;
        state.inst[i].pos.X += state.vel[i].X * frame_time;
        state.inst[i].pos.Y += state.vel[i].Y * frame_time;
        state.inst[i].pos.Z += state.vel[i].Z * frame_time;
        // bounce back from 'ground'
        if (state.inst[i].pos.Y < -2.0f) {
            state.inst[i].pos.Y = -1.8f;
            state.vel[i].Y = -state.vel[i].Y;
            state.vel[i].X *= 0.8f; state.vel[i].Y *= 0.8f; state.vel[i].Z *= 0.8f;
        }
    }
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
        .window_title = "instancing-pull-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
