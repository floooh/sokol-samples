//------------------------------------------------------------------------------
//  instancing-metal.c
//------------------------------------------------------------------------------
#include "stdlib.h"
#include "osxentry.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

#define WIDTH (640)
#define HEIGHT (480)
#define SAMPLE_COUNT (4)
#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float ry;
    hmm_mat4 view_proj;
    /* particle positions and velocity */
    int cur_num_particles;
    hmm_vec3 pos[MAX_PARTICLES];
    hmm_vec3 vel[MAX_PARTICLES];
} state = {
    /* a pass-action to clear to black */
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
    }
};

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static void init(void) {
    /* setup sokol_gfx */
    sg_setup(&(sg_desc){
        .context = osx_get_context()
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
        .content = vertices,
    });

    /* index buffer for static geometry */
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    });

    /* empty, dynamic instance-data vertex buffer, goes into vertex-buffer-slot 1 */
    state.bind.vertex_buffers[1] = sg_make_buffer(&(sg_buffer_desc){
        .size = MAX_PARTICLES * sizeof(hmm_vec3),
        .usage = SG_USAGE_STREAM
    });

    /* a shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .vs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float3 pos [[attribute(0)]];\n"
            "  float4 color [[attribute(1)]];\n"
            "  float3 instance_pos [[attribute(2)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float4 color;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  float4 pos = float4(in.pos + in.instance_pos, 1.0);\n"
            "  out.pos = params.mvp * pos;\n"
            "  out.color = in.color;\n"
            "  return out;\n"
            "}\n",
        .fs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "}\n"
    });

    /* a pipeline object */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* vertex buffer at slot 1 must step per instance */
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },  /* position */
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },  /* color */
                [2] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 },  /* instance_pos */
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true,
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
        }
    });

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    state.view_proj = HMM_MultiplyMat4(proj, view);
}

static void frame(void) {
    const float frame_time = 1.0f / 60.0f;

    /* emit new particles */
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (state.cur_num_particles < MAX_PARTICLES) {
            state.pos[state.cur_num_particles] = HMM_Vec3(0.0, 0.0, 0.0);
            state.vel[state.cur_num_particles] = HMM_Vec3(
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) * 0.5f + 2.0f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f);
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
    sg_update_buffer(state.bind.vertex_buffers[1], state.pos, state.cur_num_particles*sizeof(hmm_vec3));

    /* model-view-projection matrix */
    state.ry += 1.0f;
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(state.view_proj, HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));;

    /* ...and draw */
    sg_begin_default_pass(&state.pass_action, osx_width(), osx_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 24, state.cur_num_particles);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, SAMPLE_COUNT, "Sokol Instancing (Metal)", init, frame, shutdown);
    return 0;
}
