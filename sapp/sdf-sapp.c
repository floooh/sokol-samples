//------------------------------------------------------------------------------
//  sdf-sapp.c
//
//  Signed-distance-field rendering demo to test the shader code generate
//  with some non-trivial code.
//
//  https://www.iquilezles.org/www/articles/mandelbulb/mandelbulb.htm
//  https://www.shadertoy.com/view/ltfSWn
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "sdf-sapp.glsl.h"

static struct {
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    vs_params_t vs_params;
} state;

void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    // a vertex buffer to render a 'fullscreen triangle'
    float fsq_verts[] = { -1.0f, -3.0f, 3.0f, 1.0f, -1.0f, 1.0f };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(fsq_verts),
        .label = "fsq vertices"
    });

    // shader and pipeline object for rendering a fullscreen quad
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2
        },
        .shader = sg_make_shader(sdf_shader_desc(sg_query_backend())),
    });

    // don't need to clear since the whole framebuffer is overwritten
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_DONTCARE }
    };
}

void frame(void) {
    int w = sapp_width();
    int h = sapp_height();
    state.vs_params.time += (float)sapp_frame_duration();
    state.vs_params.aspect = (float)w / (float)h;
    sg_begin_default_pass(&state.pass_action, w, h);
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(state.vs_params));
    sg_draw(0, 3, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 512,
        .height = 512,
        .gl_force_gles2 = true,
        .window_title = "SDF Rendering",
        .icon.sokol_default = true,
    };
}
