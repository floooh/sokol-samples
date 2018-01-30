//------------------------------------------------------------------------------
//  instancing-d3d11.c
//  Demonstrates instanced rendering through D3D11 backend.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_D3D11_SHADER_COMPILER
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

enum {
    MAX_PARTICLES = 512 * 1024,
    NUM_PARTICLES_EMITTED_PER_FRAME = 10,
};

/* vertex shader uniform block */
typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

/* particle positions and velocity */
int cur_num_particles = 0;
hmm_vec3 pos[MAX_PARTICLES];
hmm_vec3 vel[MAX_PARTICLES];

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    /* setup d3d11 app wrapper and sokol_gfx */
    const int width = 800;
    const int height = 600;
    const int sample_count = 4;
    d3d11_init(width, height, sample_count, L"Sokol Instancing D3D11");
    sg_setup(&(sg_desc){
        .d3d11_device = d3d11_device(),
        .d3d11_device_context = d3d11_device_context(),
        .d3d11_render_target_view_cb = d3d11_render_target_view,
        .d3d11_depth_stencil_view_cb = d3d11_depth_stencil_view
    });

    /* vertex buffer for static geometry (goes into vb slot 0) */
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
    sg_buffer vbuf_geom = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    });

    /* index buffer for static geometry */
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    sg_buffer ibuf_geom = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    });
    
    /* dynamic per-instance data, goes into vb slot 1 */
    sg_buffer vbuf_inst = sg_make_buffer(&(sg_buffer_desc){
        .size = MAX_PARTICLES * sizeof(hmm_vec3),
        .usage = SG_USAGE_STREAM
    });

    /* create shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .vs.source = 
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
            "};\n",
        .fs.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
            "};\n"
    });

    /* pipeline object, note the vertex layout description */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        /* NOTE: the strides and attribute offsets here are not necessary,
           since both buffers have no gaps between vertices, it's just here
           for better understanding what's going on :)
        */
        .layout = {
            .buffers = {
                [0] = { .stride = 28 },
                [1] = { .stride = 12, .step_func = SG_VERTEXSTEP_PER_INSTANCE }
            },
            .attrs = {
                [0] = { .sem_name="POSITION", .offset=0,  .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },
                [1] = { .sem_name="COLOR",    .offset=12, .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },
                [2] = { .sem_name="INSTPOS",  .offset=0,  .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = sample_count
        }
    });

    /* draw state with resource bindings */
    sg_draw_state draw_state = {
        .pipeline = pip,
        .vertex_buffers = {
            [0] = vbuf_geom,
            [1] = vbuf_inst
        },
        .index_buffer = ibuf_geom
    };

    /* default pass action (clear to grey) */
    sg_pass_action pass_action = { 0 };

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)width/(float)height, 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* draw loop */
    vs_params_t vs_params;
    float roty = 0.0f;
    const float frame_time = 1.0f / 60.0f;
    while (d3d11_process_events()) {
        /* emit new particles */
        for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
            if (cur_num_particles < MAX_PARTICLES) {
                pos[cur_num_particles] = HMM_Vec3(0.0, 0.0, 0.0);
                vel[cur_num_particles] = HMM_Vec3(
                    ((float)(rand() & 0x7FFF) / (float)0x7FFF) - 0.5f,
                    ((float)(rand() & 0x7FFF) / (float)0x7FFF) * 0.5f + 2.0f,
                    ((float)(rand() & 0x7FFF) / (float)0x7FFF) - 0.5f);
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
            if (pos[i].Y < -2.0f) {
                pos[i].Y = -1.8f;
                vel[i].Y = -vel[i].Y;
                vel[i].X *= 0.8f; vel[i].Y *= 0.8f; vel[i].Z *= 0.8f;
            }
        }

        /* update dynamic instance data buffer */
        sg_update_buffer(vbuf_inst, pos, cur_num_particles*sizeof(hmm_vec3));

        /* model-view-projection matrix */
        roty += 1.0f;
        vs_params.mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(roty, HMM_Vec3(0.0f, 1.0f, 0.0f)));;

        sg_begin_default_pass(&pass_action, d3d11_width(), d3d11_height());
        sg_apply_draw_state(&draw_state);
        sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
        sg_draw(0, 24, cur_num_particles);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
}


