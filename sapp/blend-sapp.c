//------------------------------------------------------------------------------
//  blend-sapp.c
//  Test/demonstrate blend modes.
//------------------------------------------------------------------------------
#include <assert.h>
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "blend-sapp.glsl.h"

#define NUM_BLEND_FACTORS (15)

static struct {
    sg_pass_action pass_action;
    sg_bindings bind;
    sg_pipeline pips[NUM_BLEND_FACTORS][NUM_BLEND_FACTORS];
    sg_pipeline bg_pip;
    float r;
    quad_vs_params_t quad_vs_params;
    bg_fs_params_t bg_fs_params;
} state;

void init(void) {
    sg_setup(&(sg_desc){
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1,
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    /* a default pass action which does not clear, since the entire screen is overwritten anyway */
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
    sg_shader bg_shd = sg_make_shader(bg_shader_desc(sg_query_backend()));

    /* a pipeline state object for rendering the background quad */
    state.bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        /* we use the same vertex buffer as for the colored 3D quads,
           but only the first two floats from the position, need to
           provide a stride to skip the gap to the next vertex
        */
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [ATTR_vs_bg_position].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = bg_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    /* a shader for the blended quads */
    sg_shader quad_shd = sg_make_shader(quad_shader_desc(sg_query_backend()));

    /* one pipeline object per blend-factor combination */
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [ATTR_vs_quad_position].format=SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_quad_color0].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = quad_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend_color = { 1.0f, 0.0f, 0.0f, 1.0f },
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
                pip_desc.colors[0].blend = (sg_blend_state){
                    .enabled = true,
                    .src_factor_rgb = src_blend,
                    .dst_factor_rgb = dst_blend,
                    .src_factor_alpha = SG_BLENDFACTOR_ONE,
                    .dst_factor_alpha = SG_BLENDFACTOR_ZERO
                };
                state.pips[src][dst] = sg_make_pipeline(&pip_desc);
                assert(state.pips[src][dst].id != SG_INVALID_ID);
            }
            else {
                state.pips[src][dst].id = SG_INVALID_ID;
            }
        }
    }
}

void frame(void) {
    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(90.0f, sapp_widthf()/sapp_heightf(), 0.01f, 100.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 25.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* start rendering */
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());

    /* draw a background quad */
    sg_apply_pipeline(state.bg_pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_bg_fs_params, &SG_RANGE(state.bg_fs_params));
    sg_draw(0, 4, 1);

    /* draw the blended quads */
    float r0 = state.r;
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
            if (state.pips[src][dst].id != SG_INVALID_ID) {
                /* compute new model-view-proj matrix */
                hmm_mat4 rm = HMM_Rotate(r0, HMM_Vec3(0.0f, 1.0f, 0.0f));
                const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
                const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
                hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
                state.quad_vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

                sg_apply_pipeline(state.pips[src][dst]);
                sg_apply_bindings(&state.bind);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_quad_vs_params, &SG_RANGE(state.quad_vs_params));
                sg_draw(0, 4, 1);
            }
        }
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.r += 0.6f * t;
    state.bg_fs_params.tick += 1.0f * t;
}

void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .gl_force_gles2 = true,
        .window_title = "Blend Modes (sokol-app)",
        .icon.sokol_default = true,
    };
}
