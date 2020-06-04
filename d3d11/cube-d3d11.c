//------------------------------------------------------------------------------
//  cube-d3d11.c
//  Demonstrate uniform block updates.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

/* a uniform block with a model-view-projection matrix */
typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    /* setup d3d11 app wrapper and sokol_gfx */
    const int sample_count = 4;
    const int width = 800;
    const int height = 600;
    d3d11_init(width, height, sample_count, L"Sokol Cube D3D11");
    sg_setup(&(sg_desc){
        .context = d3d11_get_context()
    });

    /* cube vertex buffer */
    float vertices[] = {
        /* positions        colors */
        -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
        -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

        -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

        1.0, -1.0, -1.0,   1.0, 0.5, 0.0, 1.0,
        1.0,  1.0, -1.0,   1.0, 0.5, 0.0, 1.0,
        1.0,  1.0,  1.0,   1.0, 0.5, 0.0, 1.0,
        1.0, -1.0,  1.0,   1.0, 0.5, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

        -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
        -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* cube indices */
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20, 23, 22, 20
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* define the resource bindings */
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* create shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0] = { .sem_name = "POSITION" },
            [1] = { .sem_name = "COLOR", .sem_index=1 }
        },
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .source =
                "cbuffer params: register(b0) {\n"
                "  float4x4 mvp;\n"
                "};\n"
                "struct vs_in {\n"
                "  float4 pos: POSITION;\n"
                "  float4 color: COLOR1;\n"
                "};\n"
                "struct vs_out {\n"
                "  float4 color: COLOR0;\n"
                "  float4 pos: SV_Position;\n"
                "};\n"
                "vs_out main(vs_in inp) {\n"
                "  vs_out outp;\n"
                "  outp.pos = mul(mvp, inp.pos);\n"
                "  outp.color = inp.color;\n"
                "  return outp;\n"
                "};\n",
        },
        .fs.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
            "}\n"
    });

    /* a pipeline object */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        /* test to provide buffer stride, but no attribute offsets */
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_FLOAT4
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
        }
    });

    /* default pass action (clear to gray) */
    sg_pass_action pass_action = { 0 };

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)width/(float)height, 0.01, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    float rx = 0.0f, ry = 0.0f;
    while (d3d11_process_events()) {
        /* model-view-proj matrix for vertex shader */
        vs_params_t vs_params;
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        sg_begin_default_pass(&pass_action, d3d11_width(), d3d11_height());
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}

