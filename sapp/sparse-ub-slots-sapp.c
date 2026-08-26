//------------------------------------------------------------------------------
//  sparse-ub-slots-sapp.c
//
//  Test uniform block usage with bindslot gaps, ported from
//  https://github.com/bgourlie/sokol-wgpu-sparse-uniform-repro
//
//  (also see: https://github.com/floooh/sokol/issues/1586)
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"
#include "sparse-ub-slots-sapp.glsl.h"

static struct {
    sg_bindings bind;
    sg_pipeline pip_dense;
    sg_pipeline pip_sparse;
    sg_pass_action pass_action;
} state = {
    .pass_action.colors[0] = {
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = { 0.15f, 0.15f, 0.18f, 1.0f },
    },
};

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric(),
        .logger.func = slog_func,
    });

    static const float vertices[] = {
        0.0f, 0.65f,
        0.45f, -0.65f,
        -0.45f, -0.65f
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "triangle-vertices",
    });
    state.pip_dense = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(dense_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_dense_position].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .label = "pip-dense",
    });
    state.pip_sparse = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(sparse_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_sparse_position].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .label = "pip-sparse",
    });
}

static void frame(void) {
    sdtx_canvas(sapp_width() * 0.5f, sapp_height() * 0.5f);
    sdtx_origin(2.0f, 2.0f);
    sdtx_home();
    sdtx_color3f(0.0f, 1.0f, 0.0f);
    sdtx_puts("Left triangle should be green\n\n");
    sdtx_color3f(1.0f, 0.0f, 1.0f);
    sdtx_puts("Right triangle should be magenta\n");

    const vs_params_t left_vs_params = { .offset = { -1.0f, 0.0f } };
    const vs_params_t right_vs_params = { .offset = { 1.0f, 0.0f } };
    const dense_fs_params_t dense_fs_params = { .color = { 0.0, 1.0, 0.0, 1.0} };
    const sparse_fs_params_t sparse_fs_params = { .color = { 1.0, 0.0, 1.0, 1.0} };

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    // left triangle, dense uniform block slots
    sg_apply_pipeline(state.pip_dense);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(left_vs_params));
    sg_apply_uniforms(UB_dense_fs_params, &SG_RANGE(dense_fs_params));
    sg_draw(0, 3, 1);
    // right triangle, sparse uniform block slots
    sg_apply_pipeline(state.pip_sparse);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(right_vs_params));
    sg_apply_uniforms(UB_sparse_fs_params, &SG_RANGE(sparse_fs_params));
    sg_draw(0, 3, 1);
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sdtx_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 450,
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .icon.sokol_default = true,
        .window_title = "sparse-ub-slot-sapp.c",
        .logger.func = slog_func,
    };
}
