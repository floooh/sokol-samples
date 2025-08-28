//------------------------------------------------------------------------------
//  blend-emsc.c
//  Test blend factor combinations.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

typedef struct {
    mat44_t mvp;
} vs_params_t;

typedef struct {
    float tick;
} fs_params_t;

#define NUM_BLEND_FACTORS (15)
static struct {
    sg_bindings bind;;
    sg_pipeline pips[NUM_BLEND_FACTORS][NUM_BLEND_FACTORS];
    sg_pipeline bg_pip;
    float r;
    fs_params_t fs_params;
    sg_pass_action pass_action;
} state;

static EM_BOOL draw(double time, void* userdata);

int main() {
    // setup WebGL context
    emsc_init("#canvas", EMSC_ANTIALIAS);

    // setup sokol_gfx (need to increase pipeline pool size)
    sg_setup(&(sg_desc){
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1,
        .environment = emsc_environment(),
        .logger.func = slog_func,
    });

    // a pass action which does not clear, since the entire screen is overwritten anyway
    state.pass_action = (sg_pass_action) {
        .colors[0].load_action = SG_LOADACTION_DONTCARE ,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    // a quad vertex buffer
    float vertices[] = {
        // pos               color
        -1.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.5f,
        +1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.5f,
        -1.0f, +1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.5f,
        +1.0f, +1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.5f
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // a shader for the fullscreen background quad
    sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 300 es\n"
            "layout(location=0) in vec2 position;\n"
            "void main() {\n"
            "  gl_Position = vec4(position, 0.5, 1.0);\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform float tick;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);\n"
            "  frag_color = vec4(vec3(xy.x*xy.y), 1.0);\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(fs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "tick", .type = SG_UNIFORMTYPE_FLOAT }
            },
        },
    });

    // a pipeline state object for rendering the background quad
    state.bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = bg_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    // a shader for the blended quads
    sg_shader quad_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 300 es\n"
            "uniform mat4 mvp;\n"
            "layout(location=0) in vec4 position;\n"
            "layout(location=1) in vec4 color0;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;"
            "  color = color0;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 }
            }
        },
    });

    // one pipeline object per blend-factor combination
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = quad_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend_color = { 1.0f, 0.0f, 0.0f, 1.0f },
        .colors[0].write_mask = SG_COLORMASK_RGB,
    };
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++) {
            const sg_blend_factor src_blend = (sg_blend_factor) (src+1);
            const sg_blend_factor dst_blend = (sg_blend_factor) (dst+1);
            /* WebGL exceptions:
                - "GL_SRC_ALPHA_SATURATE as a destination blend function is disallowed in WebGL 1"
                - "constant color and constant alpha cannot be used together as source and
                   destination factors in the blend function"
            */
            bool valid = true;
            if (dst_blend == SG_BLENDFACTOR_SRC_ALPHA_SATURATED) {
                valid = false;
            }
            else if ((src_blend == SG_BLENDFACTOR_BLEND_COLOR) || (src_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_COLOR)) {
                if ((dst_blend == SG_BLENDFACTOR_BLEND_ALPHA) || (dst_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_ALPHA)) {
                    valid = false;
                }
            }
            else if ((src_blend == SG_BLENDFACTOR_BLEND_ALPHA) || (src_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_ALPHA)) {
                if ((dst_blend == SG_BLENDFACTOR_BLEND_COLOR) || (dst_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_COLOR)) {
                    valid = false;
                }
            }
            if (valid) {
                pip_desc.colors[0].blend = (sg_blend_state) {
                    .enabled = true,
                    .src_factor_rgb = src_blend,
                    .dst_factor_rgb = dst_blend,
                    .src_factor_alpha = SG_BLENDFACTOR_ONE,
                    .dst_factor_alpha = SG_BLENDFACTOR_ZERO,
                };
                state.pips[src][dst] = sg_make_pipeline(&pip_desc);
                assert(state.pips[src][dst].id != SG_INVALID_ID);
            }
            else {
                state.pips[src][dst].id = SG_INVALID_ID;
            }
        }
    }

    // hand off control to browser loop
    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(90.0f), (float)emsc_width()/(float)emsc_height(), 0.01f, 100.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, 20.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = emsc_swapchain() });

    // draw a background quad
    sg_apply_pipeline(state.bg_pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(state.fs_params));
    sg_draw(0, 4, 1);

    // draw the blended quads
    float r0 = state.r;
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
            const mat44_t rm = mat44_rotation_y(vm_radians(r0));
            const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
            const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
            const mat44_t model = vm_mul(rm, mat44_translation(x, y, 0.0f));
            const vs_params_t vs_params = { .mvp = vm_mul(model, view_proj) };
            if (state.pips[src][dst].id != SG_INVALID_ID) {
                sg_apply_pipeline(state.pips[src][dst]);
                sg_apply_bindings(&state.bind);
                sg_apply_uniforms(0, &SG_RANGE(vs_params));
                sg_draw(0, 4, 1);
            }
        }
    }
    sg_end_pass();
    sg_commit();
    state.r += 0.6f;
    state.fs_params.tick += 1.0f;
    return EM_TRUE;
}
