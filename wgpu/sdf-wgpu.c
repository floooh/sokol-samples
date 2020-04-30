//------------------------------------------------------------------------------
//  sdf-wgpu.c
//
//  Signed-distance-field rendering demo to test the shader code generate
//  with some non-trivial code.
//
//  https://www.iquilezles.org/www/articles/mandelbulb/mandelbulb.htm
//  https://www.shadertoy.com/view/ltfSWn
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "wgpu_entry.h"
#include "sdf-wgpu.glsl.h"

static struct {
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    vs_params_t vs_params;
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_DONTCARE }
    }
};

static void init(void) {
    sg_setup(&(sg_desc){
        .context = wgpu_get_context()
    });

    // a vertex buffer to render a 'fullscreen triangle'
    float fsq_verts[] = { -1.0f, -3.0f, 3.0f, 1.0f, -1.0f, 1.0f };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(fsq_verts),
        .content = fsq_verts,
        .label = "fsq vertices"
    });

    // shader and pipeline object for rendering a fullscreen quad
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2
        },
        .shader = sg_make_shader(sdf_shader_desc()),
    });
}

static void frame(void) {
    int w = wgpu_width();
    int h = wgpu_height();
    state.vs_params.time += 1.0f / 60.0f;
    state.vs_params.aspect = (float)w / (float)h;
    sg_begin_default_pass(&state.pass_action, w, h);
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &state.vs_params, sizeof(state.vs_params));
    sg_draw(0, 3, 1);
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
        .width = 512,
        .height = 512,
        .title = "sdf-wgpu"
    });
    return 0;
}
