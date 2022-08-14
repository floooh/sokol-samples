//------------------------------------------------------------------------------
//  spine-sapp.c
//  Simple sokol_spine.h sample
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "spine/spine.h"
#define SOKOL_SPINE_IMPL
#include "sokol_spine.h"
#include "dbgui/dbgui.h"

static struct {
    sg_pass_action pass_action;
} state;

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());

    sspine_setup(&(sspine_desc){0});

    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f} }
    };
}

static void frame(void) {
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    (void)ev; // FIXME!
}

static void cleanup(void) {
    __dbgui_shutdown();
    sspine_shutdown();
    sg_shutdown();
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
        .sample_count = 4,
        .window_title = "spine-sapp.c",
        .icon.sokol_default = true,
    };
}
