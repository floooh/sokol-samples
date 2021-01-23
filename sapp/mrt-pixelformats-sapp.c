//------------------------------------------------------------------------------
//  mrt-pixelformats-sapp.c
//
//  Test/demonstrate multiple-render-target rendering with different
//  pixel formats.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "mrt-pixelformats-sapp.glsl.h"

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());
    if (!sg_query_features().multiple_render_targets) {
        return;
    }
}

static void draw_fallback() {
    const sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.0f, 0.0f, 1.0f }}
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

static void frame(void) {
    if (!sg_query_features().multiple_render_targets) {
        draw_fallback();
        return;
    }
    sg_begin_default_pass(&(sg_pass_action){0}, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

static void event(const sapp_event* ev) {
    // FIXME: resize render targets on resize
    __dbgui_event(ev);
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 800,
        .height = 600,
        .window_title = "MRT Pixelformats"
    };
}


