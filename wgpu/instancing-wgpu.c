//------------------------------------------------------------------------------
//  instancing-wgpy.c
//  Demonstrate simple hardware-instancing using a static geometry buffer
//  and a dynamic instance-data buffer.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "wgpu_entry.h"
#include "instancing-wgpu.glsl.h"

#define SAMPLE_COUNT (4)
#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float ry;
    int cur_num_particles;
    hmm_vec3 pos[MAX_PARTICLES];
    hmm_vec3 vel[MAX_PARTICLES];
    uint32_t rand_val;
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    },
    .rand_val = 0x12345678
};

/* pseudo-random number generator */
static uint32_t xorshift32(void) {
    state.rand_val ^= state.rand_val<<13;
    state.rand_val ^= state.rand_val>>17;
    state.rand_val ^= state.rand_val<<5;
    return state.rand_val;
}

static void init(void) {
    sg_setup(&(sg_desc){
        .staging_buffer_size = MAX_PARTICLES * sizeof(hmm_vec3),
        .context = wgpu_get_context()
    });

    /* vertex buffer for static geometry, goes into vertex-buffer-slot 0 */
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
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .data = SG_RANGE(vertices)
    });

    /* index buffer for static geometry */
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "geometry-indices"
    });

    /* empty, dynamic instance-data vertex buffer, goes into vertex-buffer-slot 1 */
    state.bind.vertex_buffers[1] = sg_make_buffer(&(sg_buffer_desc){
        .size = MAX_PARTICLES * sizeof(hmm_vec3),
        .usage = SG_USAGE_STREAM,
        .label = "instance-data"
    });

    /* a shader */
    sg_shader shd = sg_make_shader(instancing_shader_desc(sg_query_backend()));

    /* a pipeline object */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* vertex buffer at slot 1 must step per instance */
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [ATTR_vs_pos]      = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },
                [ATTR_vs_color0]   = { .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },
                [ATTR_vs_inst_pos] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
        .label = "instancing-pipeline"
    });
}

static void frame(void) {
    const float frame_time = 1.0f / 60.0f;

    /* emit new particles */
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (state.cur_num_particles < MAX_PARTICLES) {
            state.pos[state.cur_num_particles] = HMM_Vec3(0.0, 0.0, 0.0);
            state.vel[state.cur_num_particles] = HMM_Vec3(
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f,
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f,
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f);
            state.cur_num_particles++;
        }
        else {
            break;
        }
    }

    /* update particle positions */
    for (int i = 0; i < state.cur_num_particles; i++) {
        state.vel[i].Y -= 1.0f * frame_time;
        state.pos[i].X += state.vel[i].X * frame_time;
        state.pos[i].Y += state.vel[i].Y * frame_time;
        state.pos[i].Z += state.vel[i].Z * frame_time;
        /* bounce back from 'ground' */
        if (state.pos[i].Y < -2.0f) {
            state.pos[i].Y = -1.8f;
            state.vel[i].Y = -state.vel[i].Y;
            state.vel[i].X *= 0.8f; state.vel[i].Y *= 0.8f; state.vel[i].Z *= 0.8f;
        }
    }

    /* update instance data */
    sg_update_buffer(state.bind.vertex_buffers[1], &(sg_range){
        .ptr=state.pos,
        .size=state.cur_num_particles*sizeof(hmm_vec3)
    });

    /* model-view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)wgpu_width()/(float)wgpu_height(), 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.ry += 1.0f;
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));;

    /* ...and draw */
    sg_begin_default_pass(&state.pass_action, wgpu_width(), wgpu_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.cur_num_particles);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .width = 640,
        .height = 480,
        .sample_count = SAMPLE_COUNT,
        .title = "instancing-wgpu"
    });
    return 0;
}
