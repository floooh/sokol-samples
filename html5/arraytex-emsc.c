//------------------------------------------------------------------------------
//  arraytex-emsc.c
//------------------------------------------------------------------------------
#include <stddef.h>     // offsetof
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_log.h"
#include "sokol_gfx.h"
#include "emsc.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

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

#define IMG_LAYERS (3)
#define IMG_WIDTH (16)
#define IMG_HEIGHT (16)

static EM_BOOL draw(double time, void* userdata);

static mat44_t compute_mvp(float rx, float ry, int width, int height) {
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)width/(float)height, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, view_proj);
}

int main() {
    // setup WebGL2 context
    emsc_init("#canvas", EMSC_ANTIALIAS);
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = emsc_environment(),
        .logger.func = slog_func,
    });
    assert(sg_isvalid());

    // a 16x16 array image and texture-view with 3 layers and a checkerboard pattern
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
            .data.subimage[0][0] = SG_RANGE(pixels)
        }),
    });

    // ...and a sampler
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    // cube vertex buffer
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
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // define the resource bindings
    state.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .views[0] = tex_view,
        .samplers[0] = smp,
    };

    // shader to sample from array texture
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
            "}\n",
        .fragment_func.source =
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
            "}\n",
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "texcoord0"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp",     .type = SG_UNIFORMTYPE_MAT4 },
                [1] = { .glsl_name = "offset0", .type = SG_UNIFORMTYPE_FLOAT2 },
                [2] = { .glsl_name = "offset1", .type = SG_UNIFORMTYPE_FLOAT2 },
                [3] = { .glsl_name = "offset2", .type = SG_UNIFORMTYPE_FLOAT2 }
            }
        },
        .views[0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .image_type = SG_IMAGETYPE_ARRAY },
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .glsl_name = "tex",
            .view_slot = 0,
            .sampler_slot = 0
        },
    });

    // create pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK
    });

    // default pass action
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action=SG_LOADACTION_CLEAR, .clear_value={0.0f, 0.0f, 0.0f, 1.0f} }
    };

    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    // rotated model matrix
    state.rx += 0.25f; state.ry += 0.5f;
    float offset = (float)state.frame_index * 0.0001f;
    state.frame_index++;
    const vs_params_t vs_params = {
        .mvp = compute_mvp(state.rx, state.ry, emsc_width(), emsc_height()),
        .offset0 = vec2(-offset, offset),
        .offset1 = vec2(offset, -offset),
        .offset2 = vec2(0.0f, 0.0f),
    };
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = emsc_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
    state.frame_index++;
    return EM_TRUE;
}
