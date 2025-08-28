//------------------------------------------------------------------------------
//  mrt-emsc.c
//  Multiple-render-target sample for HTML5, this requires WebGL2.
//------------------------------------------------------------------------------
#include <stddef.h>     // offsetof
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

#define NUM_MRTS (3)

static struct {
    float rx, ry;
    sg_image color_img[NUM_MRTS];
    sg_image resolve_img[NUM_MRTS];
    sg_image depth_img;
    struct {
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_view resolve_texview[NUM_MRTS];
        sg_pass_action pass_action;
        sg_pipeline fsq_pip;
        sg_bindings fsq_bind;
        sg_pipeline dbg_pip;
        sg_bindings dbg_bind;
    } display;
} state;

typedef struct {
    float x, y, z;
    float b;
} vertex_t;

typedef struct {
    mat44_t mvp;
} offscreen_params_t;

typedef struct {
    vec2_t offset;
} params_t;

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
    // setup WebGL context
    emsc_init("#canvas", EMSC_ANTIALIAS);

    // setup sokol_gfx
    sg_desc desc = {
        .environment = emsc_environment(),
        .logger.func = slog_func,
    };
    sg_setup(&desc);
    assert(sg_isvalid());

    // create base images for color, resolve and depth-stencil attachments
    // and texture views for the resolve images
    const int width = 640;
    const int height = 480;
    const int offscreen_sample_count = 4;
    for (int i = 0; i < NUM_MRTS; i++) {
        state.color_img[i] = sg_make_image(&(sg_image_desc){
            .usage.color_attachment = true,
            .width = width,
            .height = height,
            .sample_count = offscreen_sample_count,
        });
        state.resolve_img[i] = sg_make_image(&(sg_image_desc){
            .usage.resolve_attachment = true,
            .width = width,
            .height = height,
            .sample_count = 1,
        });
        state.display.resolve_texview[i] = sg_make_view(&(sg_view_desc){
            .texture.image = state.resolve_img[i],
        });
    }
    state.depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = width,
        .height = height,
        .sample_count = offscreen_sample_count,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
    });

    // setup pass struct for the offscreen pass (pass action and attachment views)
    state.offscreen.pass = (sg_pass){
        .attachments = {
            .colors = {
                [0] = sg_make_view(&(sg_view_desc){ .color_attachment.image = state.color_img[0] }),
                [1] = sg_make_view(&(sg_view_desc){ .color_attachment.image = state.color_img[1] }),
                [2] = sg_make_view(&(sg_view_desc){ .color_attachment.image = state.color_img[2] }),
            },
            .resolves = {
                [0] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = state.resolve_img[0] }),
                [1] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = state.resolve_img[1] }),
                [2] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = state.resolve_img[2] }),
            },
            .depth_stencil = sg_make_view(&(sg_view_desc){ .depth_stencil_attachment.image = state.depth_img }),
        },
        .action = {
            .colors = {
                [0] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.25f, 0.0f, 0.0f, 1.0f }
                },
                [1] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.0f, 0.25f, 0.0f, 1.0f }
                },
                [2] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.0f, 0.0f, 0.25f, 1.0f }
                }
            }
        }
    };

    // cube vertex buffer
    vertex_t cube_vertices[] = {
        // pos + brightness
        { -1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f,  1.0f, -1.0f,   1.0f },
        { -1.0f,  1.0f, -1.0f,   1.0f },

        { -1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f,  1.0f,  1.0f,   0.8f },
        { -1.0f,  1.0f,  1.0f,   0.8f },

        { -1.0f, -1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f,  1.0f,   0.6f },
        { -1.0f, -1.0f,  1.0f,   0.6f },

        { 1.0f, -1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f,  1.0f,    0.4f },
        { 1.0f, -1.0f,  1.0f,    0.4f },

        { -1.0f, -1.0f, -1.0f,   0.5f },
        { -1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f, -1.0f,   0.5f },

        { -1.0f,  1.0f, -1.0f,   0.7f },
        { -1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f, -1.0f,   0.7f },
    };
    sg_buffer cube_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices)
    });

    // index buffer for the cube
    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer cube_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices)
    });

    // resource bindings for offscreen rendering
    state.offscreen.bind = (sg_bindings){
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    // a shader to render the cube into offscreen MRT render targets
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 300 es\n"
            "uniform mat4 mvp;\n"
            "in vec4 position;\n"
            "in float bright0;\n"
            "out float bright;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  bright = bright0;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in float bright;\n"
            "layout(location=0) out vec4 frag_color_0;\n"
            "layout(location=1) out vec4 frag_color_1;\n"
            "layout(location=2) out vec4 frag_color_2;\n"
            "void main() {\n"
            "  frag_color_0 = vec4(bright, 0.0, 0.0, 1.0);\n"
            "  frag_color_1 = vec4(0.0, bright, 0.0, 1.0);\n"
            "  frag_color_2 = vec4(0.0, 0.0, bright, 1.0);\n"
            "}\n",
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "bright0"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(offscreen_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 }
            }
        },
    });

    // pipeline object for the offscreen-rendered cube
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [0] = { .offset=offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset=offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .color_count = 3,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = offscreen_sample_count
    });

    // a vertex buffer to render a fullscreen rectangle
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_buf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices)
    });

    // a sampler with linear filtering and clamp-to-edge
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    // resource bindings to render the fullscreen quad
    state.display.fsq_bind = (sg_bindings){
        .vertex_buffers[0] = quad_buf,
        .views = {
            [0] = state.display.resolve_texview[0],
            [1] = state.display.resolve_texview[1],
            [2] = state.display.resolve_texview[2],
        },
        .samplers[0] = smp,
    };

    // a shader to render a fullscreen rectangle, which 'composes'
    // the 3 offscreen render target images onto the screen
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 300 es\n"
            "uniform vec2 offset;"
            "in vec2 pos;\n"
            "out vec2 uv0;\n"
            "out vec2 uv1;\n"
            "out vec2 uv2;\n"
            "void main() {\n"
            "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
            "  uv0 = pos + vec2(offset.x, 0.0);\n"
            "  uv1 = pos + vec2(0.0, offset.y);\n"
            "  uv2 = pos;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform sampler2D tex0;\n"
            "uniform sampler2D tex1;\n"
            "uniform sampler2D tex2;\n"
            "in vec2 uv0;\n"
            "in vec2 uv1;\n"
            "in vec2 uv2;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  vec3 c0 = texture(tex0, uv0).xyz;\n"
            "  vec3 c1 = texture(tex1, uv1).xyz;\n"
            "  vec3 c2 = texture(tex2, uv2).xyz;\n"
            "  frag_color = vec4(c0 + c1 + c2, 1.0);\n"
            "}\n",
        .attrs[0].glsl_name = "pos",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "offset", .type = SG_UNIFORMTYPE_FLOAT2}
            }
        },
        .views = {
            [0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
            [1].texture.stage = SG_SHADERSTAGE_FRAGMENT,
            [2].texture.stage = SG_SHADERSTAGE_FRAGMENT,
        },
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .texture_sampler_pairs = {
            [0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex0", .view_slot = 0, .sampler_slot = 0 },
            [1] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex1", .view_slot = 1, .sampler_slot = 0 },
            [2] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex2", .view_slot = 2, .sampler_slot = 0 },
        },
    });

    // the pipeline object for the fullscreen rectangle
    state.display.fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    });

    // and prepare a pipeline and bindings to render a debug visualizations
    //  of the offscreen render target contents
    state.display.dbg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(&(sg_shader_desc){
            .vertex_func.source =
                "#version 300 es\n"
                "in vec2 pos;\n"
                "out vec2 uv;\n"
                "void main() {\n"
                "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
                "  uv = pos;\n"
                "}\n",
            .fragment_func.source =
                "#version 300 es\n"
                "precision mediump float;\n"
                "uniform sampler2D tex;\n"
                "in vec2 uv;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = vec4(texture(tex,uv).xyz, 1.0);\n"
                "}\n",
            .attrs[0].glsl_name = "pos",
            .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
            .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
            .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex", .view_slot = 0, .sampler_slot = 0 },
        })
    });
    state.display.dbg_bind = (sg_bindings) {
        .vertex_buffers[0] = quad_buf,
        .samplers[0] = smp,
        // images will be filled right before rendering
    };

    // default pass action, no clear needed, since whole screen is overwritten
    state.display.pass_action = (sg_pass_action){
        .colors[0].load_action = SG_LOADACTION_DONTCARE,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    // hand off control to browser-loop
    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    state.rx += 1.0f; state.ry += 2.0f;
    const offscreen_params_t offscreen_params = {
        .mvp = compute_mvp(state.rx, state.ry, emsc_width(), emsc_height()),
    };
    const params_t params = {
        .offset = vec2(vm_sin(state.rx * 0.01f) * 0.1f, vm_sin(state.ry * 0.01f) * 0.1f)
    };

    // render cube into MRT offscreen render targets
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(0, &SG_RANGE(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // render fullscreen quad with the 'composed image', plus 3 small
    // debug-views with the content of the offscreen render targets
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = emsc_swapchain()
    });
    sg_apply_pipeline(state.display.fsq_pip);
    sg_apply_bindings(&state.display.fsq_bind);
    sg_apply_uniforms(0, &SG_RANGE(params));
    sg_draw(0, 4, 1);
    sg_apply_pipeline(state.display.dbg_pip);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        state.display.dbg_bind.views[0] = state.display.resolve_texview[i];
        sg_apply_bindings(&state.display.dbg_bind);
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}
