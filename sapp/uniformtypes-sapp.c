//------------------------------------------------------------------------------
//  uniformtypes-sapp.c
//  Test sokol-gfx uniform types and uniform block memory layout.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "uniformtypes-sapp.glsl.h"

static struct {
    sg_pass_action pass_action;
} state;

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());
    
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.5f, 0.75f, 1.0f } }
    };
}

static void frame(void) {
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
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
        .height = 600,
        .window_title = "Uniform Types",
        .icon.sokol_default = true,
    };
}
