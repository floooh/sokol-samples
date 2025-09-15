//------------------------------------------------------------------------------
//  arraytex-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

#define WIDTH (640)
#define HEIGHT (480)
#define SAMPLE_COUNT (4)
#define IMG_LAYERS  (3)
#define IMG_WIDTH   (16)
#define IMG_HEIGHT  (16)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry;
    int frame_index;
} state;

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
    // setup sokol
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    // a 16x16 array image with 3 layers and a checkerboard pattern
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
    state.bind.views[0] = sg_make_view(&(sg_view_desc){
        .texture.image = sg_make_image(&(sg_image_desc){
            .type = SG_IMAGETYPE_ARRAY,
            .width = IMG_WIDTH,
            .height = IMG_HEIGHT,
            .num_slices = IMG_LAYERS,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data.mip_levels[0] = SG_RANGE(pixels),
        }),
    });
    state.bind.samplers[0] = sg_make_sampler(&(sg_sampler_desc) {
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
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
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
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
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // shader to sample from array texture
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
        .fragment_func.source =
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
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .image_type = SG_IMAGETYPE_ARRAY,
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
    });

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT2 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_NONE,
    });

    // default pass action (clear to black)
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value={0.0f, 0.0f, 0.0f, 1.0f} }
    };
}

static void frame() {
    // rotated model matrix
    state.rx += 0.25f; state.ry += 0.5f;
    float offset = (float)state.frame_index * 0.0001f;
    state.frame_index++;
    const vs_params_t vs_params = {
        .mvp = compute_mvp(state.rx, state.ry, osx_width(), osx_height()),
        .offset0 = vec2(-offset, offset),
        .offset1 = vec2(offset, -offset),
        .offset2 = vec2(0.0f, 0.0f),
    };
    // render the frame
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = osx_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, SAMPLE_COUNT, SG_PIXELFORMAT_DEPTH, "arraytex-metal", init, frame, shutdown);
    return 0;
}
