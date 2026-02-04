//------------------------------------------------------------------------------
//  blend-playground-sapp.c
//
//  Test/demonstrate blend state configuration.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#include "blend-playground-sapp.glsl.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline bg_pip;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };

    // create pipeline and shader to draw a bufferless fullscreen triangle as background
    state.bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(bg_shader_desc(sg_query_backend())),
        .label = "background-pipeline"
    });
}

static void frame(void) {
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });

    // draw background
    const bg_params_t bg_params = { .light = 0.6f, .dark = 0.4f };
    sg_apply_pipeline(state.bg_pip);
    sg_apply_uniforms(UB_bg_params, &SG_RANGE(bg_params));
    sg_draw(0, 3, 1);

    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    (void)ev;
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 1,
        .window_title = "blend-playground-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
