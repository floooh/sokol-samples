//------------------------------------------------------------------------------
//  spine-layers-sapp.c
//  Demonstrates sokol_spine.h layered rendering mixed with sokol_gl.h
//  rendering.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#define SOKOL_GL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "spine/spine.h"
#include "sokol_spine.h"
#include "sokol_gl.h"
#include "sokol_glue.h"
#include "stb/stb_image.h"
#include "util/fileutil.h"
#include "dbgui/dbgui.h"

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instance;
    sg_pass_action pass_action;
} state;

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    sspine_setup(&(sspine_desc){0});
    sgl_setup(&(sgl_desc_t){0});
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
    });
    __dbgui_setup(sapp_sample_count());

    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.625f, 0.08f, 0.33f, 1.0f } }
    };
}

static void frame(void) {
    sfetch_dowork();
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sfetch_shutdown();
    sgl_shutdown();
    sspine_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 1024,
        .height = 768,
        .window_title = "spine-layers-sapp.c",
        .icon.sokol_default = true
    };
}
