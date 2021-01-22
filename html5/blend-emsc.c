//------------------------------------------------------------------------------
//  blend-emsc.c
//  Test blend factor combinations.
//------------------------------------------------------------------------------
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"
#include "emsc.h"

typedef struct {
    hmm_mat4 mvp;
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
    vs_params_t vs_params;
    fs_params_t fs_params;
    sg_pass_action pass_action;
} state;

static void draw();

int main() {
    /* setup WebGL context */
    emsc_init("#canvas", EMSC_ANTIALIAS);

    /* setup sokol_gfx (need to increase pipeline pool size) */
    sg_setup(&(sg_desc){
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1
    });

    /* a pass action which does not clear, since the entire screen is overwritten anyway */
    state.pass_action = (sg_pass_action) {
        .colors[0].action = SG_ACTION_DONTCARE ,
        .depth.action = SG_ACTION_DONTCARE,
        .stencil.action = SG_ACTION_DONTCARE
    };

    /* a quad vertex buffer */
    float vertices[] = {
        /* pos               color */
        -1.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.5f,
        +1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.5f,
        -1.0f, +1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.5f,
        +1.0f, +1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.5f
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    /* a shader for the fullscreen background quad */
    sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].name = "position"
        },
        .vs.source =
            "attribute vec2 position;\n"
            "void main() {\n"
            "  gl_Position = vec4(position, 0.5, 1.0);\n"
            "}\n",
        .fs = {
            .uniform_blocks[0] = {
                .size = sizeof(fs_params_t),
                .uniforms = {
                    [0] = { .name="tick", .type=SG_UNIFORMTYPE_FLOAT }
                }
            },
            .source =
                "precision mediump float;\n"
                "uniform float tick;\n"
                "void main() {\n"
                "  vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);\n"
                "  gl_FragColor = vec4(vec3(xy.x*xy.y), 1.0);\n"
                "}\n"
        }
    });

    /* a pipeline state object for rendering the background quad */
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

    /* a shader for the blended quads */
    sg_shader quad_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].name = "position",
            [1].name = "color0"
        },
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .vs.source =
            "uniform mat4 mvp;\n"
            "attribute vec4 position;\n"
            "attribute vec4 color0;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;"
            "  color = color0;\n"
            "}\n",
        .fs.source =
            "precision mediump float;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "  gl_FragColor = color;\n"
            "}"
    });

    /* one pipeline object per blend-factor combination */
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

    /* hand off control to browser loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

void draw() {
    /* compute view-proj matrix from current width/height */
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)emsc_width()/(float)emsc_height(), 0.01f, 100.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 25.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    sg_begin_default_pass(&state.pass_action, emsc_width(), emsc_height());

    /* draw a background quad */
    sg_apply_pipeline(state.bg_pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(state.fs_params));
    sg_draw(0, 4, 1);

    /* draw the blended quads */
    float r0 = state.r;
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
            hmm_mat4 rm = HMM_Rotate(r0, HMM_Vec3(0.0f, 1.0f, 0.0f));
            const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
            const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
            hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
            state.vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

            if (state.pips[src][dst].id != SG_INVALID_ID) {
                sg_apply_pipeline(state.pips[src][dst]);
                sg_apply_bindings(&state.bind);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(state.vs_params));
                sg_draw(0, 4, 1);
            }
        }
    }
    sg_end_pass();
    sg_commit();
    state.r += 0.6f;
    state.fs_params.tick += 1.0f;
}
