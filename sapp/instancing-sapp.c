//------------------------------------------------------------------------------
//  instancing.c
//  Demonstrate simple hardware-instancing using a static geometry buffer
//  and a dynamic instance-data buffer.
//------------------------------------------------------------------------------
#include <stdlib.h> /* rand() */
#include "sokol_app.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

static const char *vs_src, *fs_src;

#define MSAA_SAMPLES (4)
#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

/* a pass-action to clear to black */
static sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
};
static sg_pipeline pip;
static sg_bindings bind;
static float ry;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

/* particle positions and velocity */
static int cur_num_particles = 0;
static hmm_vec3 pos[MAX_PARTICLES];
static hmm_vec3 vel[MAX_PARTICLES];

void init(void) {
    sg_setup(&(sg_desc){
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
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
    bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    });

    /* index buffer for static geometry */
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    });

    /* empty, dynamic instance-data vertex buffer, goes into vertex-buffer-slot 1 */
    bind.vertex_buffers[1] = sg_make_buffer(&(sg_buffer_desc){
        .size = MAX_PARTICLES * sizeof(hmm_vec3),
        .usage = SG_USAGE_STREAM
    });

    /* a shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .vs.source = vs_src,
        .fs.source = fs_src
    });

    /* a pipeline object */
    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* vertex buffer at slot 1 must step per instance */
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [0] = { .name="position",     .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },
                [1] = { .name="color0",       .sem_name="COLOR",    .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },
                [2] = { .name="instance_pos", .sem_name="INSTPOS",  .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 }
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
}

void frame(void) {
    const float frame_time = 1.0f / 60.0f;

    /* emit new particles */
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (cur_num_particles < MAX_PARTICLES) {
            pos[cur_num_particles] = HMM_Vec3(0.0, 0.0, 0.0);
            vel[cur_num_particles] = HMM_Vec3(
                ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f,
                ((float)(rand() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f,
                ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f);
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
    sg_update_buffer(bind.vertex_buffers[1], pos, cur_num_particles*sizeof(hmm_vec3));

    /* model-view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    ry += 1.0f;
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));;

    /* ...and draw */
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 24, cur_num_particles);
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .sample_count = MSAA_SAMPLES,
        .gl_force_gles2 = true,
        .window_title = "Instancing (sokol-app)",
    };
}

#if defined(SOKOL_GLCORE33)
static const char* vs_src =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec3 position;\n"
    "in vec4 color0;\n"
    "in vec3 instance_pos;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "  vec4 pos = vec4(position + instance_pos, 1.0);"
    "  gl_Position = mvp * pos;\n"
    "  color = color0;\n"
    "}\n";
static const char* fs_src =
    "#version 330\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = color;\n"
    "}\n";
#elif defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
static const char* vs_src =
    "uniform mat4 mvp;\n"
    "attribute vec3 position;\n"
    "attribute vec4 color0;\n"
    "attribute vec3 instance_pos;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  vec4 pos = vec4(position + instance_pos, 1.0);"
    "  gl_Position = mvp * pos;\n"
    "  color = color0;\n"
    "}\n";
static const char* fs_src =
    "precision mediump float;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  gl_FragColor = color;\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char* vs_src =
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
    "}\n";
static const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "fragment float4 _main(float4 color [[stage_in]]) {\n"
    "  return color;\n"
    "}\n";
#elif defined(SOKOL_D3D11)
static const char* vs_src =
    "cbuffer params: register(b0) {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float3 pos: POSITION;\n"
    "  float4 color: COLOR0;\n"
    "  float3 inst_pos: INSTPOS;\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 color: COLOR0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = mul(mvp, float4(inp.pos + inp.inst_pos, 1.0));\n"
    "  outp.color = inp.color;\n"
    "  return outp;\n"
    "};\n";
static const char* fs_src =
    "float4 main(float4 color: COLOR0): SV_Target0 {\n"
    "  return color;\n"
    "};\n";
#endif
