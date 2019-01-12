//------------------------------------------------------------------------------
//  offscreen-sapp.c
//  Render to an offscreen rendertarget texture, and use this texture
//  for rendering to the display.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

static const int MSAA_SAMPLES = 4;

static const char *offscreen_vs_src, *offscreen_fs_src, *default_vs_src, *default_fs_src;

static sg_pass offscreen_pass;
static sg_pipeline offscreen_pip;
static sg_bindings offscreen_bind;
static sg_pipeline default_pip;
static sg_bindings default_bind;

/* offscreen: clear to black */
static sg_pass_action offscreen_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
};

/* display: clear to blue-ish */
static sg_pass_action default_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.25f, 1.0f, 1.0f } }
};

/* rotation angles */
static float rx, ry;

/* vertex-shader params (just a model-view-projection matrix) */
typedef struct {
    hmm_mat4 mvp;
} params_t;

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

    /* a render pass with one color- and one depth-attachment image */
    sg_image_desc img_desc = {
        .render_target = true,
        .width = 256,
        .height = 256,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .sample_count = MSAA_SAMPLES
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    sg_image depth_img = sg_make_image(&img_desc);
    offscreen_pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = color_img,
        .depth_stencil_attachment.image = depth_img
    });

    /* cube vertex buffer with positions, colors and tex coords */
    float vertices[] = {
        /* pos                  color                       uvs */
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
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* an index buffer for the cube */
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
        .content = indices
    });

    /* a shader for a non-textured cube, rendered in the offscreen pass */
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .vs.source = offscreen_vs_src,
        .fs.source = offscreen_fs_src
    });

    /* ...and another shader for the display-pass, rendering a textured cube
       using the offscreen render target as texture */
    sg_shader default_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .vs.source = default_vs_src,
        .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
        .fs.source = default_fs_src
    });

    /* pipeline-state-object for offscreen-rendered cube, don't need texture coord here */
    offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* need to provide stride, because the buffer's texcoord is skipped */
            .buffers[0].stride = 36,
            /* but don't need to provide attr offsets, because pos and color are continuous */
            .attrs = {
                [0] = { .name="position", .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="color0", .sem_name="COLOR", .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true,
        },
        .blend = {
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = MSAA_SAMPLES
        }
    });

    /* and another pipeline-state-object for the default pass */
    default_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* don't need to provide buffer stride or attr offsets, no gaps here */
            .attrs = {
                [0] = { .name="position", .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="color0", .sem_name="COLOR", .format=SG_VERTEXFORMAT_FLOAT4 },
                [2] = { .name="texcoord0", .sem_name="TEXCOORD", .format=SG_VERTEXFORMAT_FLOAT2 }
            }
        },
        .shader = default_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = MSAA_SAMPLES
        }
    });

    /* the resource bindings for rendering a non-textured cube into offscreen render target */
    offscreen_bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* resource bindings to render a textured cube, using the offscreen render target as texture */
    default_bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = color_img
    };
}

void frame(void) {
    /* compute model-view-projection matrix for vertex shader, this will be
       used both for the offscreen-pass, and the display-pass */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    params_t vs_params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    /* the offscreen pass, rendering an rotating, untextured cube into a render target image */
    sg_begin_pass(offscreen_pass, &offscreen_pass_action);
    sg_apply_pipeline(offscreen_pip);
    sg_apply_bindings(&offscreen_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    /* and the display-pass, rendering a rotating, textured cube, using the
       previously rendered offscreen render-target as texture */
    sg_begin_default_pass(&default_pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(default_pip);
    sg_apply_bindings(&default_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
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
        .window_title = "Offscreen Rendering (sokol-app)",
    };
}

#if defined(SOKOL_GLCORE33)
static const char* offscreen_vs_src =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec4 position;\n"
    "in vec4 color0;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  color = color0;\n"
    "}\n";
static const char* offscreen_fs_src =
    "#version 330\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = color;\n"
    "}\n";
static const char* default_vs_src =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec4 position;\n"
    "in vec4 color0;\n"
    "in vec2 texcoord0;\n"
    "out vec4 color;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  color = color0;\n"
    "  uv = texcoord0;\n"
    "}\n";
static const char* default_fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec4 color;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = texture(tex, uv) + color * 0.5;\n"
    "}\n";
#elif defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
static const char* offscreen_vs_src =
    "uniform mat4 mvp;\n"
    "attribute vec4 position;\n"
    "attribute vec4 color0;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  color = color0;\n"
    "}\n";
static const char* offscreen_fs_src =
    "precision mediump float;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  gl_FragColor = color;\n"
    "}\n";
static const char* default_vs_src =
    "uniform mat4 mvp;\n"
    "attribute vec4 position;\n"
    "attribute vec4 color0;\n"
    "attribute vec2 texcoord0;\n"
    "varying vec4 color;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  color = color0;\n"
    "  uv = texcoord0;\n"
    "}\n";
static const char* default_fs_src =
    "precision mediump float;"
    "uniform sampler2D tex;\n"
    "varying vec4 color;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, uv) + color * 0.5;\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char* offscreen_vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 position [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float4 color;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.position;\n"
    "  out.color = in.color;\n"
    "  return out;\n"
    "}\n";
static const char* offscreen_fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "fragment float4 _main(float4 color [[stage_in]]) {\n"
    "  return color;\n"
    "};\n";
static const char* default_vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 position [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "  float2 uv [[attribute(2)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.position;\n"
    "  out.color = in.color;\n"
    "  out.uv = in.uv;\n"
    "  return out;\n"
    "}\n";
static const char* default_fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return float4(tex.sample(smp, in.uv).xyz + in.color.xyz * 0.5, 1.0);\n"
    "};\n";
#elif defined(SOKOL_D3D11)
static const char* offscreen_vs_src =
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
    "}\n";
static const char* offscreen_fs_src =
    "float4 main(float4 color: COLOR0): SV_Target0 {\n"
    "  return color;\n"
    "}\n";
static const char* default_vs_src =
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
    "}\n";
static const char* default_fs_src =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float4 color: COLOR0, float2 uv: TEXCOORD0): SV_Target0 {\n"
    "  return tex.Sample(smp, uv) + color * 0.5;\n"
    "}\n";
#endif
