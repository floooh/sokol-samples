//------------------------------------------------------------------------------
//  inject-wgpu.c
//
//  Demonstrates injection of native WebGPU buffers and textures into sokol-gfx.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"

// technically webgpu.h was already included by the sokol_gfx.h implementation, but
// this won't be the case when sokol_gfx.h is included without SOKOL_IMPL
#include <webgpu/webgpu.h>

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
    uint32_t pixels[IMG_WIDTH*IMG_HEIGHT];
    WGPUBuffer wgpu_vbuf;
    WGPUBuffer wgpu_ibuf;
    WGPUTexture wgpu_tex;
    WGPUTextureView wgpu_tex_view;
    WGPUSampler wgpu_smp;
} state;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static size_t roundup(size_t val, size_t to) {
    return (val + (to - 1)) & ~(to - 1);
}

static void init(void) {
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .context = wgpu_get_context(),
        .logger.func = slog_func,
    });
    WGPUDevice wgpu_dev = (WGPUDevice) sg_wgpu_device();

    // pass action to clear to black
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1 } }
    };

    // WebGPU vertex and index buffer
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

    state.wgpu_vbuf = wgpuDeviceCreateBuffer(wgpu_dev, &(WGPUBufferDescriptor){
        .label = "wgpu-vertex-buffer",
        .size = sizeof(vertices),
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = true,
    });
    void* vbuf_ptr = wgpuBufferGetMappedRange(state.wgpu_vbuf, 0, sizeof(vertices));
    assert(vbuf_ptr);
    memcpy(vbuf_ptr, vertices, sizeof(vertices));
    wgpuBufferUnmap(state.wgpu_vbuf);

    state.wgpu_ibuf = wgpuDeviceCreateBuffer(wgpu_dev, &(WGPUBufferDescriptor){
        .label = "wgpu-index-buffer",
        .size = roundup(sizeof(indices), 4),
        .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = true,
    });
    void* ibuf_ptr = wgpuBufferGetMappedRange(state.wgpu_ibuf, 0, roundup(sizeof(indices), 4));
    assert(ibuf_ptr);
    memcpy(ibuf_ptr, indices, sizeof(indices));
    wgpuBufferUnmap(state.wgpu_ibuf);

    // important to call sg_reset_state_cache() after calling WebGPU functions directly
    sg_reset_state_cache();

    // create sokol_gfx buffers with inject WebGPU buffer object
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .size = sizeof(vertices),
        .wgpu_buffer = state.wgpu_vbuf,
    });
    assert(sg_wgpu_query_buffer_info(state.bind.vertex_buffers[0]).buf == state.wgpu_vbuf);
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .wgpu_buffer = state.wgpu_ibuf,
    });
    assert(sg_wgpu_query_buffer_info(state.bind.index_buffer).buf == state.wgpu_ibuf);

    // create dynamically updated WebGPU texture object
    state.wgpu_tex = wgpuDeviceCreateTexture(wgpu_dev, &(WGPUTextureDescriptor) {
        .label = "wgpu-texture",
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = IMG_WIDTH,
            .height = IMG_HEIGHT,
            .depthOrArrayLayers = 1,
        },
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
    });
    state.wgpu_tex_view = wgpuTextureCreateView(state.wgpu_tex, &(WGPUTextureViewDescriptor) {
        .mipLevelCount = 1,
        .arrayLayerCount = 1,
    });
    sg_reset_state_cache();

    // ... and the sokol-gfx texture object with the injected WGPU texture
    state.bind.fs.images[0] = sg_make_image(&(sg_image_desc){
        .usage = SG_USAGE_STREAM,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .wgpu_texture = state.wgpu_tex,
        .wgpu_texture_view = state.wgpu_tex_view,
    });
    assert(sg_wgpu_query_image_info(state.bind.fs.images[0]).tex == state.wgpu_tex);
    assert(sg_wgpu_query_image_info(state.bind.fs.images[0]).view == state.wgpu_tex_view);

    // a WebGPU sampler object...
    state.wgpu_smp = wgpuDeviceCreateSampler(wgpu_dev, &(WGPUSamplerDescriptor){
        .label = "wgpu-sampler",
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .maxAnisotropy = 1,
    });
    sg_reset_state_cache();

    // ...and a matching sokol-gfx sampler with injected WebGPU sampler
    state.bind.fs.samplers[0] = sg_make_sampler(&(sg_sampler_desc){
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wgpu_sampler = state.wgpu_smp,
    });
    assert(sg_wgpu_query_sampler_info(state.bind.fs.samplers[0]).smp == state.wgpu_smp);

    // a sokol-gfx shader object
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .source =
                "struct vs_params {\n"
                "  mvp: mat4x4f,\n"
                "}\n"
                "@group(0) @binding(0) var<uniform> in: vs_params;\n"
                "struct vs_out {\n"
                "  @builtin(position) pos: vec4f,\n"
                "  @location(0) uv: vec2f,\n"
                "}\n"
                "@vertex fn main(@location(0) pos: vec4f, @location(1) uv: vec2f) -> vs_out {\n"
                "  var out: vs_out;\n"
                "  out.pos = in.mvp * pos;\n"
                "  out.uv = uv;\n"
                "  return out;\n"
                "}\n"
        },
        .fs = {
            .images[0].used = true,
            .samplers[0].used = true,
            .image_sampler_pairs[0] = { .used = true, .image_slot = 0, .sampler_slot = 0 },
            .source =
                "@group(1) @binding(32) var tex: texture_2d<f32>;\n"
                "@group(1) @binding(48) var smp: sampler;\n"
                "@fragment fn main(@location(0) uv: vec2f) -> @location(0) vec4f {\n"
                "  return textureSample(tex, smp, uv);\n"
                "}\n"
        },
    });
    assert(sg_wgpu_query_shader_info(shd).vs_mod != 0);
    assert(sg_wgpu_query_shader_info(shd).fs_mod != 0);
    assert(sg_wgpu_query_shader_info(shd).bgl != 0);

    // a pipeline state object
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT2 }
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
    assert(sg_wgpu_query_pipeline_info(state.pip).pip != 0);
}

void frame() {
    // view-projection matrix
    const hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    const hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // model-view-projection matrix for vertex shader
    state.rx += 1.0f; state.ry += 2.0f;
    const hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    const hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    const vs_params_t vs_params = {
        .mvp = HMM_MultiplyMat4(view_proj, model)
    };

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
    sg_image_data content = { .subimage[0][0] = SG_RANGE(state.pixels) };
    sg_update_image(state.bind.fs.images[0], &content);

    sg_begin_default_pass(&state.pass_action, wgpu_width(), wgpu_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
    wgpuBufferRelease(state.wgpu_vbuf);
    wgpuBufferRelease(state.wgpu_ibuf);
    wgpuTextureRelease(state.wgpu_tex);
    wgpuSamplerRelease(state.wgpu_smp);
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = SAMPLE_COUNT,
        .width = 640,
        .height = 480,
        .title = "inject-wgpu"
    });
    return 0;
}
