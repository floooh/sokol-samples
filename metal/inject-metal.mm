//------------------------------------------------------------------------------
//  inject-metal.m
//------------------------------------------------------------------------------
#include "osxentry.h"
#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#define WIDTH (640)
#define HEIGHT (480)
#define SAMPLE_COUNT (4)
#define IMG_WIDTH (32)
#define IMG_HEIGHT (32)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry;
    uint32_t counter;
    hmm_mat4 view_proj;
    uint32_t pixels[IMG_WIDTH*IMG_HEIGHT];
} state;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static void init(void) {
    /* setup sokol_gfx */
    sg_desc desc = {
        .context = osx_get_context()
    };
    sg_setup(&desc);

    /* create native Metal vertex- and index-buffer */
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
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    id<MTLBuffer> mtl_vbuf = [osx_mtl_device()
        newBufferWithBytes:vertices length:sizeof(vertices)
        options:MTLResourceStorageModeShared];
    id<MTLBuffer> mtl_ibuf = [osx_mtl_device()
        newBufferWithBytes:indices length:sizeof(indices)
        options:MTLResourceStorageModeShared];
        
    /* important to call sg_reset_state_cache() after calling Metal functions directly */
    sg_reset_state_cache();

    /* create sokol_gfx buffers with injected Metal buffer objects */
    sg_buffer_desc vbuf_desc = {
        .size = sizeof(vertices),
        .mtl_buffers[0] = (__bridge const void*) mtl_vbuf
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
    sg_buffer_desc ibuf_desc = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .mtl_buffers[0] = (__bridge const void*) mtl_ibuf
    };
    state.bind.index_buffer = sg_make_buffer(&ibuf_desc);

    /* create dynamically updated Metal texture objects, these will
       be rotated through by sokol_gfx as they are updated, so we need
       to create SG_NUM_INFLIGHT_FRAME textures
    */
    MTLTextureDescriptor* mtl_tex_desc = [[MTLTextureDescriptor alloc] init];
    mtl_tex_desc.textureType = MTLTextureType2D;
    mtl_tex_desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    mtl_tex_desc.width = IMG_WIDTH;
    mtl_tex_desc.height = IMG_HEIGHT;
    mtl_tex_desc.mipmapLevelCount = 1;
    id<MTLTexture> mtl_tex[SG_NUM_INFLIGHT_FRAMES];
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        mtl_tex[i] = [osx_mtl_device() newTextureWithDescriptor:mtl_tex_desc];
    }
    sg_image_desc img_desc = {
        .usage = SG_USAGE_STREAM,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
    };
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        img_desc.mtl_textures[i] = (__bridge const void*) mtl_tex[i];
    }
    sg_reset_state_cache();
    state.bind.fs_images[0] = sg_make_image(&img_desc);

    /* a shader */
    sg_shader_desc shader_desc = {
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .entry = "vs_main",
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct params_t {\n"
                "  float4x4 mvp;\n"
                "};\n"
                "struct vs_in {\n"
                "  float4 position [[attribute(0)]];\n"
                "  float2 uv [[attribute(1)]];\n"
                "};\n"
                "struct vs_out {\n"
                "  float4 pos [[position]];\n"
                "  float2 uv;\n"
                "};\n"
                "vertex vs_out vs_main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
                "  vs_out out;\n"
                "  out.pos = params.mvp * in.position;\n"
                "  out.uv = in.uv;\n"
                "  return out;\n"
                "}\n"
        },
        .fs = {
            .images[0].type = SG_IMAGETYPE_2D,
            .entry = "fs_main",
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct fs_in {\n"
                "  float2 uv;\n"
                "};\n"
                "fragment float4 fs_main(fs_in in [[stage_in]],\n"
                "  texture2d<float> tex [[texture(0)]],\n"
                "  sampler smp [[sampler(0)]])\n"
                "{\n"
                "  return float4(tex.sample(smp, in.uv).xyz, 1.0);\n"
                "};\n"
        }
    };
    sg_shader shd = sg_make_shader(&shader_desc);

    /* a pipeline state object */
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT2 }
            },
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
    };
    state.pip = sg_make_pipeline(&pip_desc);

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    state.view_proj = HMM_MultiplyMat4(proj, view);
}

static void frame(void) {
    /* compute model-view-projection matrix for vertex shader */
    vs_params_t vs_params;
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(state.view_proj, model);

    /* update texture image with some generated pixel data */
    for (int y = 0; y < IMG_WIDTH; y++) {
        for (int x = 0; x < IMG_HEIGHT; x++) {
            state.pixels[y * IMG_WIDTH + x] = 0xFF000000 |
                         (state.counter & 0xFF)<<16 |
                         ((state.counter*3) & 0xFF)<<8 |
                         ((state.counter*23) & 0xFF);
            state.counter++;
        }
    }
    state.counter++;
    sg_image_content content = {
        .subimage[0][0] = { .ptr = state.pixels, .size = sizeof(state.pixels) }
    };
    sg_update_image(state.bind.fs_images[0], &content);

    sg_begin_default_pass(&state.pass_action, osx_width(), osx_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, SAMPLE_COUNT, "Sokol Resource Injection (Metal)", init, frame, shutdown);
    return 0;
}



