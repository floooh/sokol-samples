//------------------------------------------------------------------------------
//  blend-op-sapp.c
//  Test/demonstrate blend ops.
//------------------------------------------------------------------------------
#include <assert.h>
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "dbgui/dbgui.h"
#include "blend-op-sapp.glsl.h"

#define NUM_BLEND_OPS (5)

static struct {
    sg_pass_action pass_action;
    sg_bindings bind;
    sg_pipeline pips[NUM_BLEND_OPS][NUM_BLEND_OPS];
    sg_pipeline bg_pip;
    float r;
    quad_vs_params_t quad_vs_params;
    bg_fs_params_t bg_fs_params;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .pipeline_pool_size = NUM_BLEND_OPS * NUM_BLEND_OPS + 16,
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // a default pass action which does not clear, since the entire screen is overwritten anyway
    state.pass_action = (sg_pass_action) {
        .colors[0].load_action = SG_LOADACTION_DONTCARE ,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    // a quad vertex buffer
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

    // a shader for the fullscreen background quad
    sg_shader bg_shd = sg_make_shader(bg_shader_desc(sg_query_backend()));

    // a pipeline state object for rendering the background quad
    state.bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        // we use the same vertex buffer as for the colored 3D quads,
        // but only the first two floats from the position, need to
        // provide a stride to skip the gap to the next vertex
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [ATTR_bg_position].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = bg_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    // a shader for the blended quads
    sg_shader quad_shd = sg_make_shader(quad_shader_desc(sg_query_backend()));

    // one pipeline object per blend-op combination
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [ATTR_quad_position].format=SG_VERTEXFORMAT_FLOAT3,
                [ATTR_quad_color0].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = quad_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend_color = { 1.0f, 0.0f, 0.0f, 1.0f },
    };
    for (int rgb = 0; rgb < NUM_BLEND_OPS; rgb++) {
        for (int alpha = 0; alpha < NUM_BLEND_OPS; alpha++) {
            const sg_blend_op rgb_op = (sg_blend_op) (rgb+1);
            const sg_blend_op alpha_op = (sg_blend_op) (alpha+1);
            pip_desc.colors[0].blend = (sg_blend_state){
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_rgb = rgb_op,
                .op_alpha = alpha_op,
            };
            // blend-op MIN/MAX requires blend-factors to be ONE
            // (just not initializing the blend factors would also work, in that
            // case sokol-gfx will use blend factor ONE as the defaults)
            if ((rgb_op == SG_BLENDOP_MIN) || (rgb_op == SG_BLENDOP_MAX)) {
                pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
                pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
            }
            if ((alpha_op == SG_BLENDOP_MIN) || (alpha_op == SG_BLENDOP_MAX)) {
                pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
                pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE;
            }
            state.pips[rgb][alpha] = sg_make_pipeline(&pip_desc);
            assert(state.pips[rgb][alpha].id != SG_INVALID_ID);
        }
    }
}

static void frame(void) {
    // view-projection matrix
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(90.0f), sapp_widthf()/sapp_heightf(), 0.01f, 100.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, 7.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);

    // start rendering
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });

    /* draw a background quad */
    sg_apply_pipeline(state.bg_pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_bg_fs_params, &SG_RANGE(state.bg_fs_params));
    sg_draw(0, 4, 1);

    // draw the blended quads
    float r0 = state.r;
    for (int src = 0; src < NUM_BLEND_OPS; src++) {
        for (int dst = 0; dst < NUM_BLEND_OPS; dst++, r0+=0.6f) {
            // compute new model-view-proj matrix
            mat44_t rm = mat44_rotation_y(vm_radians(r0));
            const float x = ((float)(dst - NUM_BLEND_OPS/2)) * 3.0f;
            const float y = ((float)(src - NUM_BLEND_OPS/2)) * 2.2f;
            mat44_t model = vm_mul(rm, mat44_translation(x, y, 0.0f));
            state.quad_vs_params.mvp = vm_mul(model, view_proj);

            sg_apply_pipeline(state.pips[src][dst]);
            sg_apply_bindings(&state.bind);
            sg_apply_uniforms(UB_quad_vs_params, &SG_RANGE(state.quad_vs_params));
            sg_draw(0, 4, 1);
        }
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.r += 0.6f * t;
    state.bg_fs_params.tick += 1.0f * t;
}

static void cleanup(void) {
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
        .window_title = "Blend Ops (sokol-app)",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
