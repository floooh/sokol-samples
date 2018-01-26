//------------------------------------------------------------------------------
//  arraytex-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

const int WIDTH = 640;
const int HEIGHT = 480;
const int MSAA_SAMPLES = 4;

sg_draw_state draw_state;
sg_pass_action pass_action;
hmm_mat4 view_proj;
float rx = 0.0f, ry = 0.0f;
int frame_index = 0;

typedef struct {
    hmm_mat4 mvp;
    hmm_vec2 offset0;
    hmm_vec2 offset1;
    hmm_vec2 offset2;
} vs_params_t;

const int IMG_LAYERS = 3;
const int IMG_WIDTH = 16;
const int IMG_HEIGHT = 16;

void init(const void* mtl_device) {
    /* setup sokol */
    sg_setup(&(sg_desc){
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    });

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
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .vs.source =
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
            "}\n",
        .fs.images[0].type = SG_IMAGETYPE_ARRAY,
        .fs.source =
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
            "}\n"
    });

    /* a pipeline object */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT2 }
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

    /* populate the draw state struct */
    draw_state = (sg_draw_state) {
        .pipeline = pip,
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = img
    };

    /* default pass action (clear to black) */
    pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val={0.0f, 0.0f, 0.0f, 1.0f} }
    };

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);
}

void frame() {
    /* rotated model matrix */
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
    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, MSAA_SAMPLES, "Sokol Array Texture (Metal)", init, frame, shutdown);
    return 0;
}
