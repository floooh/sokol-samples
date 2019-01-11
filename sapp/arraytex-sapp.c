//------------------------------------------------------------------------------
//  arraytex-sapp.c
//  2D array texture creation and rendering.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

static const char *vs_src, *fs_src;

#define MSAA_SAMPLES (4)
#define IMG_LAYERS (3)
#define IMG_WIDTH (16)
#define IMG_HEIGHT (16)

static sg_pipeline pip;
static sg_bindings bind;
static float rx, ry;
static int frame_index;

/* default pass action (clear to black) */
static const sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val={0.0f, 0.0f, 0.0f, 1.0f} }
};

typedef struct {
    hmm_mat4 mvp;
    hmm_vec2 offset0;
    hmm_vec2 offset1;
    hmm_vec2 offset2;
} vs_params_t;

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
    if (sapp_gles2()) {
        /* this demo needs GLES3/WebGL */
        return;
    }

    /* a 16x16 array texture with 3 layers and a checkerboard pattern */
    static uint32_t pixels[IMG_LAYERS][IMG_HEIGHT][IMG_WIDTH];
    for (int layer=0, even_odd=0; layer<IMG_LAYERS; layer++) {
        for (int y = 0; y < IMG_HEIGHT; y++, even_odd++) {
            for (int x = 0; x < IMG_WIDTH; x++, even_odd++) {
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
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .layers = IMG_LAYERS,
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
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp",     .type=SG_UNIFORMTYPE_MAT4 },
                [1] = { .name="offset0", .type=SG_UNIFORMTYPE_FLOAT2 },
                [2] = { .name="offset1", .type=SG_UNIFORMTYPE_FLOAT2 },
                [3] = { .name="offset2", .type=SG_UNIFORMTYPE_FLOAT2 }
            }
        },
        .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_ARRAY },
        .vs.source = vs_src,
        .fs.source = fs_src
    });

    /* a pipeline object */
    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .name="position", .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="texcoord0", .sem_name="TEXCOORD", .format=SG_VERTEXFORMAT_FLOAT2 }
            } 
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_NONE,
            .sample_count = MSAA_SAMPLES
        }
    });

    /* populate the resource bindings struct */
    bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = img
    };
}

/* this is called when GLES3/WebGL2 is not available */
void draw_gles2_fallback(void) {
    const sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } },
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

void frame(void) {
    /* can't do anything useful on GLES2/WebGL */
    if (sapp_gles2()) {
        draw_gles2_fallback();
        return;
    }

    /* rotated model matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    rx += 0.25f; ry += 0.5f;
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);

    /* model-view-projection matrix for vertex shader */
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
    /* uv offsets */
    float offset = (float)frame_index * 0.0001f;
    vs_params.offset0 = HMM_Vec2(-offset, offset);
    vs_params.offset1 = HMM_Vec2(offset, -offset);
    vs_params.offset2 = HMM_Vec2(0.0f, 0.0f);
    frame_index++;

    /* render the frame */
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
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
        .window_title = "Array Texture (sokol-app)",
    };
}

#if defined(SOKOL_GLCORE33)
static const char* vs_src =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "uniform vec2 offset0;\n"
    "uniform vec2 offset1;\n"
    "uniform vec2 offset2;\n"
    "in vec4 position;\n"
    "in vec2 texcoord0;\n"
    "out vec3 uv0;\n"
    "out vec3 uv1;\n"
    "out vec3 uv2;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  uv0 = vec3(texcoord0 + offset0, 0.0);\n"
    "  uv1 = vec3(texcoord0 + offset1, 1.0);\n"
    "  uv2 = vec3(texcoord0 + offset2, 2.0);\n"
    "}\n";
static const char* fs_src =
    "#version 330\n"
    "uniform sampler2DArray tex;\n"
    "in vec3 uv0;\n"
    "in vec3 uv1;\n"
    "in vec3 uv2;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  vec4 c0 = texture(tex, uv0);\n"
    "  vec4 c1 = texture(tex, uv1);\n"
    "  vec4 c2 = texture(tex, uv2);\n"
    "  frag_color = vec4(c0.xyz + c1.xyz + c2.xyz, 1.0);\n"
    "}\n";
#elif defined(SOKOL_GLES3)
static const char* vs_src =
    "#version 300 es\n"
    "uniform mat4 mvp;\n"
    "uniform vec2 offset0;\n"
    "uniform vec2 offset1;\n"
    "uniform vec2 offset2;\n"
    "in vec4 position;\n"
    "in vec2 texcoord0;\n"
    "out vec3 uv0;\n"
    "out vec3 uv1;\n"
    "out vec3 uv2;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  uv0 = vec3(texcoord0 + offset0, 0.0);\n"
    "  uv1 = vec3(texcoord0 + offset1, 1.0);\n"
    "  uv2 = vec3(texcoord0 + offset2, 2.0);\n"
    "}\n";
static const char* fs_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "precision lowp sampler2DArray;\n"
    "uniform sampler2DArray tex;\n"
    "in vec3 uv0;\n"
    "in vec3 uv1;\n"
    "in vec3 uv2;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  vec4 c0 = texture(tex, uv0);\n"
    "  vec4 c1 = texture(tex, uv1);\n"
    "  vec4 c2 = texture(tex, uv2);\n"
    "  frag_color = vec4(c0.xyz + c1.xyz + c2.xyz, 1.0);\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char* vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float4x4 mvp;\n"
    "  float2 offset0;\n"
    "  float2 offset1;\n"
    "  float2 offset2;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 pos [[attribute(0)]];\n"
    "  float2 uv [[attribute(1)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float3 uv0;\n"
    "  float3 uv1;\n"
    "  float3 uv2;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.pos;\n"
    "  out.uv0 = float3(in.uv + params.offset0, 0.0);\n"
    "  out.uv1 = float3(in.uv + params.offset1, 1.0);\n"
    "  out.uv2 = float3(in.uv + params.offset2, 2.0);\n"
    "  return out;\n"
    "}\n";
static const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float3 uv0;\n"
    "  float3 uv1;\n"
    "  float3 uv2;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d_array<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  float4 c0 = tex.sample(smp, in.uv0.xy, int(in.uv0.z));\n"
    "  float4 c1 = tex.sample(smp, in.uv1.xy, int(in.uv1.z));\n"
    "  float4 c2 = tex.sample(smp, in.uv2.xy, int(in.uv2.z));\n"
    "  return float4(c0.xyz + c1.xyz + c2.xyz, 1.0);\n"
    "}\n";
#elif defined(SOKOL_D3D11)
static const char* vs_src =
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
    "}\n";
static const char* fs_src =
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
    "}\n";
#endif
