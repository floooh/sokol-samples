//------------------------------------------------------------------------------
//  mrt-metal.c
//------------------------------------------------------------------------------
#include <stddef.h>     /* offsetof */
#include "osxentry.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

const int WIDTH = 640;
const int HEIGHT = 480;
const int MSAA_SAMPLES = 4;

sg_pass_desc offscreen_pass_desc;
sg_pass_action offscreen_pass_action;
sg_pass_action default_pass_action;
sg_pass offscreen_pass;
sg_draw_state offscreen_draw_state;
sg_draw_state fsq_draw_state;
sg_draw_state dbg_draw_state;

hmm_mat4 view_proj;
float rx = 0.0f;
float ry = 0.0f;

typedef struct {
    float x, y, z, b;
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} offscreen_params_t;

typedef struct {
    hmm_vec2 offset;
} params_t;

void init(const void* mtl_device) {
    /* setup sokol */
    sg_setup(&(sg_desc){
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    });

    /* a render pass with 3 color attachment images, and a depth attachment image */
    sg_image_desc color_img_desc = {
        .render_target = true,
        .width = WIDTH,
        .height = HEIGHT,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = MSAA_SAMPLES
    };
    sg_image_desc depth_img_desc = color_img_desc;
    depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    offscreen_pass_desc = (sg_pass_desc){
        .color_attachments = {
            [0].image = sg_make_image(&color_img_desc),
            [1].image = sg_make_image(&color_img_desc),
            [2].image = sg_make_image(&color_img_desc)
        },
        .depth_stencil_attachment.image = sg_make_image(&depth_img_desc)
    };
    offscreen_pass = sg_make_pass(&offscreen_pass_desc);

    /* a matching pass action with clear colors */
    offscreen_pass_action = (sg_pass_action){
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

    /* a shader to render the cube into offscreen MRT render targest */
    sg_shader cube_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0].size = sizeof(offscreen_params_t),
        .vs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 pos [[attribute(0)]];\n"
            "  float bright [[attribute(1)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float bright;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  out.pos = params.mvp * in.pos;\n"
            "  out.bright = in.bright;\n"
            "  return out;\n"
            "}\n",
        .fs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct fs_out {\n"
            "  float4 color0 [[color(0)]];\n"
            "  float4 color1 [[color(1)]];\n"
            "  float4 color2 [[color(2)]];\n"
            "};\n"
            "fragment fs_out _main(float bright [[stage_in]]) {\n"
            "  fs_out out;\n"
            "  out.color0 = float4(bright, 0.0, 0.0, 1.0);\n"
            "  out.color1 = float4(0.0, bright, 0.0, 1.0);\n"
            "  out.color2 = float4(0.0, 0.0, bright, 1.0);\n"
            "  return out;\n"
            "}\n"
    });

    /* pipeline object for the offscreen-rendered cube */
    sg_pipeline cube_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [0] = { .offset = offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset = offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
            }
        },
        .shader = cube_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .blend = {
            .color_attachment_count = 3,
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = MSAA_SAMPLES
        }
    });

    /* draw state for offscreen rendering */
    offscreen_draw_state = (sg_draw_state){
        .pipeline = cube_pip,
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    /* a vertex buffer to render a fullscreen rectangle */
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(quad_vertices),
        .content = quad_vertices
    });

    /* a shader to render a fullscreen rectangle by adding the 3 offscreen-rendered images */
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0].size = sizeof(params_t),
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct params_t {\n"
                "  float2 offset;\n"
                "};\n"
                "struct vs_in {\n"
                "  float2 pos [[attribute(0)]];\n"
                "};\n"
                "struct vs_out {\n"
                "  float4 pos [[position]];\n"
                "  float2 uv0;\n"
                "  float2 uv1;\n"
                "  float2 uv2;\n"
                "};\n"
                "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
                "  vs_out out;\n"
                "  out.pos = float4(in.pos*2.0-1.0, 0.5, 1.0);\n"
                "  out.uv0 = in.pos + float2(params.offset.x, 0.0);\n"
                "  out.uv1 = in.pos + float2(0.0, params.offset.y);\n"
                "  out.uv2 = in.pos;\n"
                "  return out;\n"
                "}\n"
        },
        .fs = {
            .images = {
                [0] = { .type = SG_IMAGETYPE_2D },
                [1] = { .type = SG_IMAGETYPE_2D },
                [2] = { .type = SG_IMAGETYPE_2D }
            },
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct fs_in {\n"
                "  float2 uv0;\n"
                "  float2 uv1;\n"
                "  float2 uv2;\n"
                "};\n"
                "fragment float4 _main(fs_in in [[stage_in]],\n"
                "  texture2d<float> tex0 [[texture(0)]], sampler smp0 [[sampler(0)]],\n"
                "  texture2d<float> tex1 [[texture(1)]], sampler smp1 [[sampler(1)]],\n"
                "  texture2d<float> tex2 [[texture(2)]], sampler smp2 [[sampler(2)]])\n"
                "{\n"
                "  float3 c0 = tex0.sample(smp0, in.uv0).xyz;\n"
                "  float3 c1 = tex1.sample(smp1, in.uv1).xyz;\n"
                "  float3 c2 = tex2.sample(smp2, in.uv2).xyz;\n"
                "  return float4(c0 + c1 + c2, 1.0);\n"
                "}\n"
        }
    });

    /* the pipeline object to render the fullscreen quad */
    sg_pipeline fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .rasterizer.sample_count = MSAA_SAMPLES
    });

    /* draw state to render a fullscreen quad */
    fsq_draw_state = (sg_draw_state){
        .pipeline = fsq_pip,
        .vertex_buffers[0] = quad_vbuf,
        .fs_images = {
            [0] = offscreen_pass_desc.color_attachments[0].image,
            [1] = offscreen_pass_desc.color_attachments[1].image,
            [2] = offscreen_pass_desc.color_attachments[2].image
        }
    };

    /* and a draw-state to render debug-visualization quads with the content
       of the offscreen render targets */
    dbg_draw_state = (sg_draw_state){
        .pipeline = sg_make_pipeline(&(sg_pipeline_desc){
            .layout = {
                .attrs[0].format = SG_VERTEXFORMAT_FLOAT2
            },
            .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
            .shader = sg_make_shader(&(sg_shader_desc){
                .vs.source =
                    "#include <metal_stdlib>\n"
                    "using namespace metal;\n"
                    "struct vs_in {\n"
                    "  float2 pos [[attribute(0)]];\n"
                    "};\n"
                    "struct vs_out {\n"
                    "  float4 pos [[position]];\n"
                    "  float2 uv;\n"
                    "};\n"
                    "vertex vs_out _main(vs_in in [[stage_in]]) {\n"
                    "  vs_out out;\n"
                    "  out.pos = float4(in.pos*2.0-1.0, 0.5, 1.0);\n"
                    "  out.uv = in.pos;\n"
                    "  return out;\n"
                    "}\n",
                .fs.images[0].type = SG_IMAGETYPE_2D,
                .fs.source =
                    "#include <metal_stdlib>\n"
                    "using namespace metal;\n"
                    "fragment float4 _main(float2 uv [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
                    "  return float4(tex.sample(smp, uv).xyz, 1.0);\n"
                    "}\n"
            }),
            .rasterizer.sample_count = MSAA_SAMPLES
        }),
        .vertex_buffers[0] = quad_vbuf
        /* images will be filled right before rendering */
    };

    /* default pass action, no clear needed, since whole screen is overwritten */
    default_pass_action = (sg_pass_action){
        .colors = {
            [0].action = SG_ACTION_DONTCARE,
            [1].action = SG_ACTION_DONTCARE,
            [2].action = SG_ACTION_DONTCARE
        },
        .depth.action = SG_ACTION_DONTCARE,
        .stencil.action = SG_ACTION_DONTCARE
    };

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);
}

void frame() {
    offscreen_params_t offscreen_params;
    params_t params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    offscreen_params.mvp = HMM_MultiplyMat4(view_proj, model);
    params.offset = HMM_Vec2(HMM_SinF(rx*0.01f)*0.1f, HMM_SinF(ry*0.01f)*0.1f);

    /* render cube into MRT offscreen render targets */
    sg_begin_pass(offscreen_pass, &offscreen_pass_action);
    sg_apply_draw_state(&offscreen_draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &offscreen_params, sizeof(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    /* render fullscreen quad with the 'composed image', plus 3
       small debug-view quads */
    sg_begin_default_pass(&default_pass_action, osx_width(), osx_height());
    sg_apply_draw_state(&fsq_draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &params, sizeof(params));
    sg_draw(0, 4, 1);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        dbg_draw_state.fs_images[0] = offscreen_pass_desc.color_attachments[i].image;
        sg_apply_draw_state(&dbg_draw_state);
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, MSAA_SAMPLES, "Sokol MRT (Metal)", init, frame, shutdown);
    return 0;
}
