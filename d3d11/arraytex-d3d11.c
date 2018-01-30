//------------------------------------------------------------------------------
//  arraytex-d3d11.c
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

typedef struct {
    hmm_mat4 mvp;
    hmm_vec2 offset0;
    hmm_vec2 offset1;
    hmm_vec2 offset2;
} params_t;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    /* setup d3d11 app wrapper and sokol_gfx */
    const int width = 800;
    const int height = 600;
    const int sample_count = 4;
    d3d11_init(width, height, sample_count, L"Sokol Array Texture D3D11");
    sg_setup(&(sg_desc){
        .d3d11_device = d3d11_device(),
        .d3d11_device_context = d3d11_device_context(),
        .d3d11_render_target_view_cb = d3d11_render_target_view,
        .d3d11_depth_stencil_view_cb = d3d11_depth_stencil_view
    });

    /* a 16x16 array texture with 3 layers and a checkerboard pattern */
    uint32_t pixels[3][16][16];
    for (int layer=0, even_odd=0; layer<3; layer++) {
        for (int y = 0; y < 16; y++, even_odd++) {
            for (int x = 0; x < 16; x++, even_odd++) {
                if (even_odd & 1) {
                    switch (layer) {
                        case 0: pixels[layer][y][x] = 0x000000FF; break;
                        case 1: pixels[layer][y][x] = 0x0000FF00; break;
                        case 2: pixels[layer][y][x] = 0x00FF0000; break;
                    }
                }
                else {
                    pixels[layer][y][x] = 0;
                }
            }
        }
    }
    sg_image img = sg_make_image(&(sg_image_desc) {
        .type = SG_IMAGETYPE_ARRAY,
        .width = 16,
        .height = 16,
        .layers = 3,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .content.subimage[0][0] = {
            .ptr = pixels,
            .size = sizeof(pixels)
        }
    });

    /* cube vertex buffer */
    float vertices[] = {
        /* pos                  uvs */
        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f, 
         1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

         1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 1.0f
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    });

    /* create an index buffer for the cube */
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    });

    /* shader to sample from array texture */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0].size = sizeof(params_t),
        .fs.images[0].type=SG_IMAGETYPE_ARRAY,
        .vs.source =
            "cbuffer params {\n"
            "  float4x4 mvp;\n"
            "  float2 offset0;\n"
            "  float2 offset1;\n"
            "  float2 offset2;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 pos: POSITION;\n"
            "  float2 uv: TEXCOORD0;\n"
            "};\n"
            "struct vs_out {\n"
            "  float3 uv0: TEXCOORD0;\n"
            "  float3 uv1: TEXCOORD1;\n"
            "  float3 uv2: TEXCOORD2;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = mul(mvp, inp.pos);\n"
            "  outp.uv0 = float3(inp.uv + offset0, 0.0);\n"
            "  outp.uv1 = float3(inp.uv + offset1, 1.0);\n"
            "  outp.uv2 = float3(inp.uv + offset2, 2.0);\n"
            "  return outp;\n"
            "}\n",
        .fs.source =
            "Texture2DArray<float4> tex: register(t0);\n"
            "sampler smp: register(s0);\n"
            "struct fs_in {\n"
            "  float3 uv0: TEXCOORD0;\n"
            "  float3 uv1: TEXCOORD1;\n"
            "  float3 uv2: TEXCOORD2;\n"
            "};\n"
            "float4 main(fs_in inp): SV_Target0 {\n"
            "  float3 c0 = tex.Sample(smp, inp.uv0).xyz;\n"
            "  float3 c1 = tex.Sample(smp, inp.uv1).xyz;\n"
            "  float3 c2 = tex.Sample(smp, inp.uv2).xyz;\n"
            "  return float4(c0+c1+c2, 1.0);\n"
            "}\n"
    });

    /* create pipeline object */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .sem_name="TEXCOORD", .format=SG_VERTEXFORMAT_FLOAT2 }
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

    /* draw state */
    sg_draw_state draw_state = {
        .pipeline = pip,
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = img
    };

    /* default pass action */
    sg_pass_action pass_action = { 
        .colors[0] = { .action=SG_ACTION_CLEAR, .val={0.0f, 0.0f, 0.0f, 1.0f} }
    };
    
    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)width/(float)height, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    params_t vs_params;
    float rx = 0.0f, ry = 0.0f;
    int frame_index = 0;
    while (d3d11_process_events()) {
        /* rotated model matrix */
        rx += 0.25f; ry += 0.5f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);

        /* model-view-projection matrix for vertex shader */
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
        /* uv offsets */
        float offset = (float)frame_index * 0.0001f;
        vs_params.offset0 = HMM_Vec2(-offset, offset);
        vs_params.offset1 = HMM_Vec2(offset, -offset);
        vs_params.offset2 = HMM_Vec2(0.0f, 0.0f);
        frame_index++;

        sg_begin_default_pass(&pass_action, d3d11_width(), d3d11_height());
        sg_apply_draw_state(&draw_state);
        sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
