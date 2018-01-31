//------------------------------------------------------------------------------
//  instancing-metal.c
//------------------------------------------------------------------------------
#include "stdlib.h"
#include "osxentry.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

const int WIDTH = 640;
const int HEIGHT = 480;
const int MSAA_SAMPLES = 4;
const int MAX_PARTICLES = 512 * 1024;
const int NUM_PARTICLES_EMITTED_PER_FRAME = 10;

/* a pass-action to clear to black */
sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
};

sg_draw_state draw_state;
float ry = 0.0f;
hmm_mat4 view_proj;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

/* particle positions and velocity */
int cur_num_particles = 0;
hmm_vec3 pos[MAX_PARTICLES];
hmm_vec3 vel[MAX_PARTICLES];

void init(const void* mtl_device) {
    /* setup sokol_gfx */
    sg_setup(&(sg_desc){
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
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
    draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    });

    /* index buffer for static geometry */
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    draw_state.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    });

    /* empty, dynamic instance-data vertex buffer, goes into vertex-buffer-slot 1 */
    draw_state.vertex_buffers[1] = sg_make_buffer(&(sg_buffer_desc){
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
    draw_state.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
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
            .sample_count = MSAA_SAMPLES
        }
    });

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);
}

void frame() {
    const float frame_time = 1.0f / 60.0f;

    /* emit new particles */
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (cur_num_particles < MAX_PARTICLES) {
            pos[cur_num_particles] = HMM_Vec3(0.0, 0.0, 0.0);
            vel[cur_num_particles] = HMM_Vec3(
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) * 0.5f + 2.0f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f);
            cur_num_particles++;
        }
        else {
            break;
        }
    }

    /* update particle positions */
    for (int i = 0; i < cur_num_particles; i++) {
        vel[i].Y -= 1.0f * frame_time;
        pos[i].X += vel[i].X * frame_time;
        pos[i].Y += vel[i].Y * frame_time;
        pos[i].Z += vel[i].Z * frame_time;
        /* bounce back from 'ground' */
        if (pos[i].Y < -2.0f) {
            pos[i].Y = -1.8f;
            vel[i].Y = -vel[i].Y;
            vel[i].X *= 0.8f; vel[i].Y *= 0.8f; vel[i].Z *= 0.8f;
        }
    }

    /* update instance data */
    sg_update_buffer(draw_state.vertex_buffers[1], pos, cur_num_particles*sizeof(hmm_vec3));

    /* model-view-projection matrix */
    ry += 1.0f;
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));;

    /* ...and draw */
    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 24, cur_num_particles);
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, MSAA_SAMPLES, "Sokol Instancing (Metal)", init, frame, shutdown);
    return 0;
}
