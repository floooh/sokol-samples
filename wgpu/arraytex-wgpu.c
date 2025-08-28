//------------------------------------------------------------------------------
//  arraytex-wgpu.c
//  2D array texture creation and rendering.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

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
    mat44_t mvp;
    vec2_t offset0;
    vec2_t offset1;
    vec2_t offset2;
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
    sg_setup(&(sg_desc){
        .environment = wgpu_environment(),
        .logger.func = slog_func,
    });

    // a 16x16 array image and texture view with 3 layers and a checkerboard pattern
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
    sg_view tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = sg_make_image(&(sg_image_desc){
            .type = SG_IMAGETYPE_ARRAY,
            .width = IMG_WIDTH,
            .height = IMG_HEIGHT,
            .num_slices = IMG_LAYERS,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data.subimage[0][0] = SG_RANGE(pixels),
            .label = "array-texture",
        }),
        .label = "array-texture-view",
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
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // shader to sample from array texture
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
        .fragment_func.source =
            "@group(1) @binding(0) var tex: texture_2d_array<f32>;\n"
            "@group(1) @binding(1) var smp: sampler;\n"
            "@fragment fn main(@location(0) uv0: vec3f, @location(1) uv1: vec3f, @location(2) uv2: vec3f) -> @location(0) vec4f {\n"
            "  var c0 = textureSample(tex, smp, uv0.xy, u32(uv0.z)).xyz;\n"
            "  var c1 = textureSample(tex, smp, uv1.xy, u32(uv1.z)).xyz;\n"
            "  var c2 = textureSample(tex, smp, uv2.xy, u32(uv2.z)).xyz;\n"
            "  return vec4f(c0 + c1 + c2, 1.0);\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .wgsl_group0_binding_n = 0,
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .image_type = SG_IMAGETYPE_ARRAY,
            .wgsl_group1_binding_n = 0,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .wgsl_group1_binding_n = 1,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
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
        .views[0] = tex_view,
        .samplers[0] = smp,
    };
}

static void frame(void) {
    // rotated model matrix
    state.rx += 0.25f; state.ry += 0.5f;
    float offset = (float)state.frame_index * 0.0001f;
    state.frame_index++;
    const vs_params_t vs_params = {
        .mvp = compute_mvp(state.rx, state.ry, wgpu_width(), wgpu_height()),
        .offset0 = vec2(-offset, offset),
        .offset1 = vec2(offset, -offset),
        .offset2 = vec2(0.0f, 0.0f),
    };
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = wgpu_swapchain() });
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
