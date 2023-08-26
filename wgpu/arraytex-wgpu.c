//------------------------------------------------------------------------------
//  arraytex-wgpu.c
//  2D array texture creation and rendering.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"

#define SAMPLE_COUNT (4)
#define IMG_LAYERS (3)
#define IMG_WIDTH (16)
#define IMG_HEIGHT (16)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry;
    int frame_index;
} state = {
    .pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value={0.0f, 0.0f, 0.0f, 1.0f} }
    }
};

typedef struct {
    hmm_mat4 mvp;
    hmm_vec2 offset0;
    hmm_vec2 offset1;
    hmm_vec2 offset2;
} vs_params_t;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = wgpu_get_context(),
        .logger.func = slog_func,
    });

    // a 16x16 array texture with 3 layers and a checkerboard pattern
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
                } else {
                    pixels[layer][y][x] = 0;
                }
            }
        }
    }
    sg_image img = sg_make_image(&(sg_image_desc) {
        .type = SG_IMAGETYPE_ARRAY,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .num_slices = IMG_LAYERS,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.subimage[0][0] = SG_RANGE(pixels),
        .label = "array-texture"
    });

    // ...and sampler
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .label = "sampler",
    });

    // cube vertex buffer
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
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });

    // create an index buffer for the cube
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
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // shader to sample from array texture
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .source =
                "struct vs_params {\n"
                "  mvp: mat4x4f,\n"
                "  offset0: vec2f,\n"
                "  offset1: vec2f,\n"
                "  offset2: vec2f,\n"
                "}\n"
                "@group(0) @binding(0) var<uniform> in: vs_params;\n"
                "struct vs_out {\n"
                "  @builtin(position) pos: vec4f,\n"
                "  @location(0) uv0: vec3f,\n"
                "  @location(1) uv1: vec3f,\n"
                "  @location(2) uv2: vec3f,\n"
                "}\n"
                "@vertex fn main(@location(0) pos: vec4f, @location(1) uv: vec2f) -> vs_out {\n"
                "  var out: vs_out;\n"
                "  out.pos = in.mvp * pos;\n"
                "  out.uv0 = vec3f(uv + in.offset0, 0.0);\n"
                "  out.uv1 = vec3f(uv + in.offset1, 1.0);\n"
                "  out.uv2 = vec3f(uv + in.offset2, 2.0);\n"
                "  return out;\n"
                "}\n",
        },
        .fs = {
            .images[0] = { .used = true, .image_type = SG_IMAGETYPE_ARRAY },
            .samplers[0] = { .used = true },
            .image_sampler_pairs[0] = { .used = true, .image_slot = 0, .sampler_slot = 0 },
            .source =
                "@group(1) @binding(32) var tex: texture_2d_array<f32>;\n"
                "@group(1) @binding(48) var smp: sampler;\n"
                "@fragment fn main(@location(0) uv0: vec3f, @location(1) uv1: vec3f, @location(2) uv2: vec3f) -> @location(0) vec4f {\n"
                "  var c0 = textureSample(tex, smp, uv0.xy, u32(uv0.z)).xyz;\n"
                "  var c1 = textureSample(tex, smp, uv1.xy, u32(uv1.z)).xyz;\n"
                "  var c2 = textureSample(tex, smp, uv2.xy, u32(uv2.z)).xyz;\n"
                "  return vec4f(c0 + c1 + c2, 1.0);\n"
                "}\n",
        },
        .label = "cube-shader",
    });

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_NONE,
        .label = "cube-pipeline"
    });

    // populate the resource bindings struct
    state.bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs = {
            .images[0] = img,
            .samplers[0] = smp,
        },
    };
}

static void frame(void) {
    // rotated model matrix
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)wgpu_width()/(float)wgpu_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.rx += 0.25f; state.ry += 0.5f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);

    // model-view-project matrix and uv offsets
    float offset = (float) state.frame_index * 0.0001f;
    const vs_params_t vs_params = {
        .mvp = HMM_MultiplyMat4(view_proj, model),
        .offset0 = HMM_Vec2(-offset, offset),
        .offset1 = HMM_Vec2(offset, -offset),
        .offset2 = HMM_Vec2(0.0f, 0.0f),
    };
    state.frame_index++;

    // render the frame
    sg_begin_default_pass(&state.pass_action, wgpu_width(), wgpu_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = SAMPLE_COUNT,
        .width = 640,
        .height = 480,
        .title = "arraytex-wgpu"
    });
    return 0;
}
