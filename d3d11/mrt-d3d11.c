//------------------------------------------------------------------------------
//  mrt-d3d11.c
//  Multiple-render-target rendering with the D3D11 backend.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

typedef struct {
    float x, y, z, b;
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} offscreen_params_t;

typedef struct {
    hmm_vec2 offset;
} params_t;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    /* setup d3d11 app wrapper and sokol_gfx */
    const int width = 800;
    const int height = 600;
    d3d11_init(width, height, 1, L"Sokol MRT D3D11");
    sg_setup(&(sg_desc){
        .context = d3d11_get_context()
    });


    /* a render pass with 3 color attachment images, and a depth attachment image */
    const int offscreen_sample_count = sg_query_features().msaa_render_targets ? 4 : 1;
    sg_image_desc color_img_desc = {
        .render_target = true,
        .width = width,
        .height = height,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = offscreen_sample_count
    };
    sg_image_desc depth_img_desc = color_img_desc;
    depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    sg_pass_desc offscreen_pass_desc = {
        .color_attachments = {
            [0].image = sg_make_image(&color_img_desc),
            [1].image = sg_make_image(&color_img_desc),
            [2].image = sg_make_image(&color_img_desc)
        },
        .depth_stencil_attachment.image = sg_make_image(&depth_img_desc)
    };
    sg_pass offscreen_pass = sg_make_pass(&offscreen_pass_desc);

    /* a matching pass action with clear colors */
    sg_pass_action offscreen_pass_action = {
        .colors = {
            [0] = { .action = SG_ACTION_CLEAR, .val = { 0.25f, 0.0f, 0.0f, 1.0f } },
            [1] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.25f, 0.0f, 1.0f } },
            [2] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.25f, 1.0f } }
        }
    };

    /* cube vertex buffer */
    vertex_t cube_vertices[] = {
        /* pos + brightness */
        { -1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f,  1.0f, -1.0f,   1.0f },
        { -1.0f,  1.0f, -1.0f,   1.0f },

        { -1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f,  1.0f,  1.0f,   0.8f },
        { -1.0f,  1.0f,  1.0f,   0.8f },

        { -1.0f, -1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f,  1.0f,   0.6f },
        { -1.0f, -1.0f,  1.0f,   0.6f },

        { 1.0f, -1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f,  1.0f,    0.4f },
        { 1.0f, -1.0f,  1.0f,    0.4f },

        { -1.0f, -1.0f, -1.0f,   0.5f },
        { -1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f, -1.0f,   0.5f },

        { -1.0f,  1.0f, -1.0f,   0.7f },
        { -1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f, -1.0f,   0.7f },
    };
    sg_buffer cube_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(cube_vertices),
        .content = cube_vertices,
    });

    /* index buffer for the cube */
    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer cube_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(cube_indices),
        .content = cube_indices,
    });

    /* a shader to render a cube into MRT offscreen render targets */
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].sem_name = "POSITION",
            [1].sem_name = "BRIGHT"
        },
        .vs.uniform_blocks[0].size = sizeof(offscreen_params_t),
        .vs.source =
            "cbuffer params: register(b0) {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 pos: POSITION;\n"
            "  float bright: BRIGHT;\n"
            "};\n"
            "struct vs_out {\n"
            "  float bright: BRIGHT;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = mul(mvp, inp.pos);\n"
            "  outp.bright = inp.bright;\n"
            "  return outp;\n"
            "}\n",
        .fs.source =
            "struct fs_out {\n"
            "  float4 c0: SV_Target0;\n"
            "  float4 c1: SV_Target1;\n"
            "  float4 c2: SV_Target2;\n"
            "};\n"
            "fs_out main(float b: BRIGHT) {\n"
            "  fs_out outp;\n"
            "  outp.c0 = float4(b, 0.0, 0.0, 1.0);\n"
            "  outp.c1 = float4(0.0, b, 0.0, 1.0);\n"
            "  outp.c2 = float4(0.0, 0.0, b, 1.0);\n"
            "  return outp;\n"
            "}\n"
    });

    /* pipeline object for the offscreen-rendered cube */
    sg_pipeline offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [0] = { .offset=offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset=offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .blend = {
            .color_attachment_count = 3,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = offscreen_sample_count
        }
    });

    /* resource bindings for offscreen rendering */
    sg_bindings offscreen_bind = {
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    /* a vertex buffer to render a fullscreen rectangle */
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_buf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(quad_vertices),
        .content = quad_vertices,
    });

    /* a shader to render a fullscreen rectangle, which 'composes'
       the 3 offscreen render target images onto the screen */
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .attrs[0].sem_name = "POSITION",
        .vs.uniform_blocks[0].size = sizeof(params_t),
        .fs.images = {
            [0].type=SG_IMAGETYPE_2D,
            [1].type=SG_IMAGETYPE_2D,
            [2].type=SG_IMAGETYPE_2D
        },
        .vs.source =
            "cbuffer params {\n"
            "  float2 offset;\n"
            "};\n"
            "struct vs_in {\n"
            "  float2 pos: POSITION;\n"
            "};\n"
            "struct vs_out {\n"
            "  float2 uv0: TEXCOORD0;\n"
            "  float2 uv1: TEXCOORD1;\n"
            "  float2 uv2: TEXCOORD2;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = float4(inp.pos*2.0-1.0, 0.5, 1.0);\n"
            "  outp.uv0 = inp.pos + float2(offset.x, 0.0);\n"
            "  outp.uv1 = inp.pos + float2(0.0, offset.y);\n"
            "  outp.uv2 = inp.pos;\n"
            "  return outp;\n"
            "}\n",
        .fs.source =
            "Texture2D<float4> tex0: register(t0);\n"
            "Texture2D<float4> tex1: register(t1);\n"
            "Texture2D<float4> tex2: register(t2);\n"
            "sampler smp0: register(s0);\n"
            "sampler smp1: register(s1);\n"
            "sampler smp2: register(s2);\n"
            "struct fs_in {\n"
            "  float2 uv0: TEXCOORD0;\n"
            "  float2 uv1: TEXCOORD1;\n"
            "  float2 uv2: TEXCOORD2;\n"
            "};\n"
            "float4 main(fs_in inp): SV_Target0 {\n"
            "  float3 c0 = tex0.Sample(smp0, inp.uv0).xyz;\n"
            "  float3 c1 = tex1.Sample(smp1, inp.uv1).xyz;\n"
            "  float3 c2 = tex2.Sample(smp2, inp.uv2).xyz;\n"
            "  float4 c = float4(c0 + c1 + c2, 1.0);\n"
            "  return c;\n"
            "}\n"
    });

    /* the pipeline object for the fullscreen rectangle */
    sg_pipeline fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout.attrs[0].format=SG_VERTEXFORMAT_FLOAT2,
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    });

    /* resource bindings for the fullscreen quad */
    sg_bindings fsq_bind = {
        .vertex_buffers[0] = quad_buf,
        .fs_images = {
            [0] = offscreen_pass_desc.color_attachments[0].image,
            [1] = offscreen_pass_desc.color_attachments[1].image,
            [2] = offscreen_pass_desc.color_attachments[2].image,
        }
    };

    /* pipeline and resource bindings to render a debug visualizations
       of the offscreen render target contents
    */
    sg_pipeline dbg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(&(sg_shader_desc){
            .attrs[0].sem_name = "POSITION",
            .vs.source =
                "struct vs_in {\n"
                "  float2 pos: POSITION;\n"
                "};\n"
                "struct vs_out {\n"
                "  float2 uv: TEXCOORD0;\n"
                "  float4 pos: SV_Position;\n"
                "};\n"
                "vs_out main(vs_in inp) {\n"
                "  vs_out outp;\n"
                "  outp.pos = float4(inp.pos*2.0-1.0, 0.5, 1.0);\n"
                "  outp.uv = inp.pos;\n"
                "  return outp;\n"
                "}\n",
            .fs.images[0].type=SG_IMAGETYPE_2D,
            .fs.source =
                "Texture2D<float4> tex: register(t0);\n"
                "sampler smp: register(s0);\n"
                "float4 main(float2 uv: TEXCOORD0): SV_Target0 {\n"
                "  return float4(tex.Sample(smp, uv).xyz, 1.0);\n"
                "}\n"
        })
    });
    sg_bindings dbg_bind = {
        .vertex_buffers[0] = quad_buf,
        /* images will be filled right before rendering */
    };

    /* default pass action, no clear needed, since whole screen is overwritten */
    sg_pass_action default_pass_action = {
        .colors = {
            [0].action = SG_ACTION_DONTCARE,
            [1].action = SG_ACTION_DONTCARE,
            [2].action = SG_ACTION_DONTCARE
        },
        .depth.action = SG_ACTION_DONTCARE,
        .stencil.action = SG_ACTION_DONTCARE
    };

    /* view-projection matrix for the offscreen-rendered cube */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)width/(float)height, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    offscreen_params_t offscreen_params;
    params_t params;
    float rx = 0.0f, ry = 0.0f;
    while (d3d11_process_events()) {
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        offscreen_params.mvp = HMM_MultiplyMat4(view_proj, model);
        params.offset = HMM_Vec2(HMM_SinF(rx*0.01f)*0.1f, HMM_SinF(ry*0.01f)*0.1f);

        /* render cube into MRT offscreen render targets */
        sg_begin_pass(offscreen_pass, &offscreen_pass_action);
        sg_apply_pipeline(offscreen_pip);
        sg_apply_bindings(&offscreen_bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &offscreen_params, sizeof(offscreen_params));
        sg_draw(0, 36, 1);
        sg_end_pass();

        /* render fullscreen quad with the 'composed image', plus 3 small
           debug-views with the content of the offscreen render targets */
        sg_begin_default_pass(&default_pass_action, d3d11_width(), d3d11_height());
        sg_apply_pipeline(fsq_pip);
        sg_apply_bindings(&fsq_bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &params, sizeof(params));
        sg_draw(0, 4, 1);
        sg_apply_pipeline(dbg_pip);
        for (int i = 0; i < 3; i++) {
            sg_apply_viewport(i*100, 0, 100, 100, false);
            dbg_bind.fs_images[0] = offscreen_pass_desc.color_attachments[i].image;
            sg_apply_bindings(&dbg_bind);
            sg_draw(0, 4, 1);
        }
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
}
