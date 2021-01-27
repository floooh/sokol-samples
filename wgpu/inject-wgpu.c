//------------------------------------------------------------------------------
//  inject-wgpu.c
//
//  Demonstrates injection of native WebGPU buffers and textures into sokol-gfx.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "wgpu_entry.h"
#include "inject-wgpu.glsl.h"

/* NOTE: webgpu.h is already included by sokol_gfx.h */

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
    WGPUBuffer wgpu_vertex_buffer;
    WGPUBuffer wgpu_index_buffer;
    WGPUTexture wgpu_texture;
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    }
};

static void init(void) {
    /* setup sokol_gfx */
    sg_setup(&(sg_desc){
        .context = wgpu_get_context()
    });
    WGPUDevice wgpu_dev = (WGPUDevice) wgpu_get_context().wgpu.device;

    /* create native WebGPU vertex- and index-buffer */
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

    WGPUCreateBufferMappedResult buf_res = wgpuDeviceCreateBufferMapped(wgpu_dev,
        &(WGPUBufferDescriptor) {
            .size = sizeof(vertices),
            .usage = WGPUBufferUsage_Vertex
        });
    state.wgpu_vertex_buffer = buf_res.buffer;
    memcpy(buf_res.data, vertices, sizeof(vertices));
    wgpuBufferUnmap(state.wgpu_vertex_buffer);

    buf_res = wgpuDeviceCreateBufferMapped(wgpu_dev,
        &(WGPUBufferDescriptor) {
            .size = sizeof(indices),
            .usage = WGPUBufferUsage_Index
        });
    state.wgpu_index_buffer = buf_res.buffer;
    memcpy(buf_res.data, indices, sizeof(indices));
    wgpuBufferUnmap(state.wgpu_index_buffer);

    /* important to call sg_reset_state_cache() after calling WebGPU functions directly */
    sg_reset_state_cache();

    /* create sokol_gfx buffers with injected WebGPU buffer objects */
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .size = sizeof(vertices),
        .wgpu_buffer = state.wgpu_vertex_buffer
    });
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .wgpu_buffer = state.wgpu_index_buffer
    });

    /* create dynamically updated WebGPU texture object */
    state.wgpu_texture = wgpuDeviceCreateTexture(wgpu_dev,
        &(WGPUTextureDescriptor) {
            .usage = WGPUTextureUsage_Sampled|WGPUTextureUsage_CopyDst,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = IMG_WIDTH,
                .height = IMG_HEIGHT,
                .depth = 1
            },
            .arrayLayerCount = 1,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .mipLevelCount = 1,
            .sampleCount = 1,
        });
    sg_reset_state_cache();

    /* ... and the sokol-gfx texture object with the injected WGPU texture */
    state.bind.fs_images[0] = sg_make_image(&(sg_image_desc){
        .usage = SG_USAGE_STREAM,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .wgpu_texture = state.wgpu_texture
    });

    /* a pipeline state object and shader */
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT2 }
            },
        },
        .shader = sg_make_shader(inject_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    };
    state.pip = sg_make_pipeline(&pip_desc);

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    state.view_proj = HMM_MultiplyMat4(proj, view);
}

void frame() {
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
    sg_image_data content = { .subimage[0][0] = SG_RANGE(state.pixels) };
    sg_update_image(state.bind.fs_images[0], &content);

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
    wgpuBufferRelease(state.wgpu_vertex_buffer);
    wgpuBufferRelease(state.wgpu_index_buffer);
    wgpuTextureRelease(state.wgpu_texture);
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



