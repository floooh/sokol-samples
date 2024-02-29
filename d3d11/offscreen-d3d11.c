//------------------------------------------------------------------------------
//  offscreen-d3d11.c
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_gfx.h"
#include "sokol_log.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d11 app wrapper and sokol_gfx
    const int width = 800;
    const int height = 600;
    const int display_sample_count = 4;
    const int offscreen_sample_count = 1;
    d3d11_init(&(d3d11_desc_t){
        .width = width,
        .height = height,
        .sample_count = display_sample_count,
        .title = L"offscreen-d3d11.c",
    });
    sg_setup(&(sg_desc){
        .environment = d3d11_environment(),
        .logger.func = slog_func,
    });

    // create one color and one depth render target image
    sg_image_desc img_desc = {
        .render_target = true,
        .width = 512,
        .height = 512,
        .sample_count = offscreen_sample_count,
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    sg_image depth_img = sg_make_image(&img_desc);

    // an offscreen pass attachments object into those images
    sg_attachments offscreen_attachments = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = color_img,
        .depth_stencil.image = depth_img
    });
    assert(sg_d3d11_query_attachments_info(offscreen_attachments).color_rtv[0] != 0);
    assert(sg_d3d11_query_attachments_info(offscreen_attachments).color_rtv[1] == 0);
    assert(sg_d3d11_query_attachments_info(offscreen_attachments).resolve_rtv[0] == 0);
    assert(sg_d3d11_query_attachments_info(offscreen_attachments).dsv != 0);

    // a sampler object for when the rendertarget image is used as texture
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    // pass action for offscreen pass, clearing to black
    sg_pass_action offscreen_pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // pass action for default pass, clearing to blue-ish
    sg_pass_action default_pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.25f, 1.0f, 1.0f } }
    };

    // cube vertex buffer with positions, colors and tex coords
    float vertices[] = {
        // pos                  color                       uvs
        -1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,

         1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,

        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 1.0f
    };
    sg_buffer_desc vbuf_desc = {
        .data = SG_RANGE(vertices)
    };
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);

    // an index buffer for the cube
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer_desc ibuf_desc = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    };
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

    // shader for non-textured cube, rendered in offscreen pass
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].sem_name = "POSITION",
            [1].sem_name = "COLOR"
        },
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .vs.source =
            "cbuffer params: register(b0) {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 pos: POSITION;\n"
            "  float4 color: COLOR0;\n"
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
            "}\n",
        .fs.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
            "}\n"
    });

    // ...and a second shader for rendering a textured cube in the default pass
    sg_shader default_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].sem_name = "POSITION",
            [1].sem_name = "COLOR",
            [2].sem_name = "TEXCOORD"
        },
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .source =
                "cbuffer params: register(b0) {\n"
                "  float4x4 mvp;\n"
                "};\n"
                "struct vs_in {\n"
                "  float4 pos: POSITION;\n"
                "  float4 color: COLOR0;\n"
                "  float2 uv: TEXCOORD0;\n"
                "};\n"
                "struct vs_out {\n"
                "  float4 color: COLOR0;\n"
                "  float2 uv: TEXCOORD0;\n"
                "  float4 pos: SV_Position;\n"
                "};\n"
                "vs_out main(vs_in inp) {\n"
                "  vs_out outp;\n"
                "  outp.pos = mul(mvp, inp.pos);\n"
                "  outp.color = inp.color;\n"
                "  outp.uv = inp.uv;\n"
                "  return outp;\n"
                "}\n",
        },
        .fs = {
            .images[0].used = true,
            .samplers[0].used = true,
            .image_sampler_pairs[0] = { .used = true, .image_slot = 0, .sampler_slot = 0 },
            .source =
                "Texture2D<float4> tex: register(t0);\n"
                "sampler smp: register(s0);\n"
                "float4 main(float4 color: COLOR0, float2 uv: TEXCOORD0): SV_Target0 {\n"
                "  return tex.Sample(smp, uv) + color * 0.5;\n"
                "}\n"
        }
    });

    // pipeline object for offscreen rendering
    sg_pipeline offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // skip the uv coords
            .buffers[0].stride = 36,
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            },
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = offscreen_sample_count,
    });

    // pipeline object for rendering textured cube in default pass
    sg_pipeline default_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4,
                [2].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = default_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    // resource bindings for offscreen rendering
    sg_bindings offscreen_bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    // resource bindings for default pass
    sg_bindings default_bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs = {
            .images[0] = color_img,
            .samplers[0] = smp,
        }
    };

    // view-projection matrix
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)width/(float)height, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // everything ready, on to the draw loop!
    vs_params_t vs_params;
    float rx = 0.0f, ry = 0.0f;
    while (d3d11_process_events()) {
        // prepare the uniform block with the model-view-projection matrix,
        // we just use the same matrix for the offscreen- and default-pass
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 model = HMM_MultiplyMat4(
            HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f)),
            HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        // offscreen pass, rendering an untextured cube
        sg_begin_pass(&(sg_pass){ .action = offscreen_pass_action, .attachments = offscreen_attachments });
        sg_apply_pipeline(offscreen_pip);
        sg_apply_bindings(&offscreen_bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();

        // default pass, render cube textured with offscreen render target image
        sg_begin_pass(&(sg_pass){ .action = default_pass_action, .swapchain = d3d11_swapchain() });
        sg_apply_pipeline(default_pip);
        sg_apply_bindings(&default_bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();

        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
