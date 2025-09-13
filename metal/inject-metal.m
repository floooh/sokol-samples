//------------------------------------------------------------------------------
//  inject-metal.mm
//------------------------------------------------------------------------------
#include "osxentry.h"
#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#define WIDTH (640)
#define HEIGHT (480)
#define SAMPLE_COUNT (4)
#define IMG_WIDTH (32)
#define IMG_HEIGHT (32)

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry;
    uint32_t counter;
    uint32_t pixels[IMG_WIDTH*IMG_HEIGHT];
} state;

typedef struct {
    mat44_t mvp;
} vs_params_t;

static mat44_t compute_mvp(float rx, float ry, int width, int height) {
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)width/(float)height, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, view_proj);
}

static void init(void) {
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger = {
            .func = slog_func,
        }
    });

    // create native Metal vertex- and index-buffer
    float vertices[] = {
        // pos                  uvs
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

    // important to call sg_reset_state_cache() after calling Metal functions directly
    sg_reset_state_cache();

    // create sokol_gfx buffers with injected Metal buffer objects
    const sg_buffer_desc vbuf_desc = {
        .size = sizeof(vertices),
        .mtl_buffers[0] = (__bridge const void*) mtl_vbuf
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
    assert(((__bridge id<MTLBuffer>) sg_mtl_query_buffer_info(state.bind.vertex_buffers[0]).buf[0]) == mtl_vbuf);
    assert(((__bridge id<MTLBuffer>) sg_mtl_query_buffer_info(state.bind.vertex_buffers[0]).buf[1]) == nil);
    const sg_buffer_desc ibuf_desc = {
        .usage.index_buffer = true,
        .size = sizeof(indices),
        .mtl_buffers[0] = (__bridge const void*) mtl_ibuf
    };
    state.bind.index_buffer = sg_make_buffer(&ibuf_desc);
    assert(((__bridge id<MTLBuffer>) sg_mtl_query_buffer_info(state.bind.index_buffer).buf[0]) == mtl_ibuf);
    assert(((__bridge id<MTLBuffer>) sg_mtl_query_buffer_info(state.bind.index_buffer).buf[1]) == nil);

    // create dynamically updated Metal texture objects, these will
    // be rotated through by sokol_gfx as they are updated, so we need
    // to create SG_NUM_INFLIGHT_FRAME textures
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
        .usage.stream_update = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
    };
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        img_desc.mtl_textures[i] = (__bridge const void*) mtl_tex[i];
    }
    sg_reset_state_cache();
    state.img = sg_make_image(&img_desc);
    assert(((__bridge id<MTLTexture>) sg_mtl_query_image_info(state.img).tex[0]) == mtl_tex[0]);
    assert(((__bridge id<MTLTexture>) sg_mtl_query_image_info(state.img).tex[1]) == mtl_tex[1]);

    // note: texture views cannot be injected
    state.bind.views[0] = sg_make_view(&(sg_view_desc){ .texture.image = state.img });

    // create a Metal sampler object and inject into a sokol-gfx sampler object
    MTLSamplerDescriptor* mtl_smp_desc = [[MTLSamplerDescriptor alloc] init];
    mtl_smp_desc.minFilter = MTLSamplerMinMagFilterLinear;
    mtl_smp_desc.magFilter = MTLSamplerMinMagFilterLinear;
    mtl_smp_desc.rAddressMode = MTLSamplerAddressModeRepeat;
    mtl_smp_desc.sAddressMode = MTLSamplerAddressModeRepeat;
    id<MTLSamplerState> mtl_smp = [osx_mtl_device() newSamplerStateWithDescriptor:mtl_smp_desc];

    const sg_sampler_desc smp_desc = {
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .mtl_sampler = mtl_smp,
    };
    sg_reset_state_cache();
    state.bind.samplers[0] = sg_make_sampler(&smp_desc);
    assert(((__bridge id<MTLSamplerState>) sg_mtl_query_sampler_info(state.bind.samplers[0]).smp) == mtl_smp);

    // a shader
    const sg_shader_desc shader_desc = {
        .vertex_func = {
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
        .fragment_func = {
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
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .msl_texture_n = 0,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .msl_sampler_n = 0,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
        },
    };
    sg_shader shd = sg_make_shader(&shader_desc);
    assert(((__bridge id<MTLLibrary>) sg_mtl_query_shader_info(shd).vertex_lib) != nil);
    assert(((__bridge id<MTLLibrary>) sg_mtl_query_shader_info(shd).fragment_lib) != nil);
    assert(((__bridge id<MTLFunction>) sg_mtl_query_shader_info(shd).vertex_func) != nil);
    assert(((__bridge id<MTLFunction>) sg_mtl_query_shader_info(shd).fragment_func) != nil);

    // a pipeline state object
    const sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT2 }
            },
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    };
    state.pip = sg_make_pipeline(&pip_desc);
    assert(((__bridge id<MTLRenderPipelineState>) sg_mtl_query_pipeline_info(state.pip).rps) != nil);
    assert(((__bridge id<MTLDepthStencilState>) sg_mtl_query_pipeline_info(state.pip).dss) != nil);
}

static void frame(void) {
    // compute model-view-projection matrix for vertex shader
    state.rx += 1.0f; state.ry += 2.0f;
    const vs_params_t vs_params = { .mvp = compute_mvp(state.rx, state.ry, osx_width(), osx_height()) };

    // update texture image with some generated pixel data
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
    sg_update_image(state.img, &(sg_image_data){ .mip_levels[0] = SG_RANGE(state.pixels) });

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = osx_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, SAMPLE_COUNT, SG_PIXELFORMAT_DEPTH_STENCIL, "inject-metal", init, frame, shutdown);
    return 0;
}
