//------------------------------------------------------------------------------
//  debugtext-sapp.c
//  Simple debug text rendering with sokol_debugtext.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#define SOKOL_DEBUGTEXT_ALL_FONTS
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"

static struct {
    sg_pass_action pass_action;
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
    }
};

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-debugtext
    sdtx_setup(&(sdtx_desc_t){
        .context = {
            .color_format = sapp_color_format(),
            .depth_format = sapp_depth_format(),
            .sample_count = sapp_sample_count()
        }
    });
}

static void frame(void) {

    // set virtual canvas size to half display size so that
    // glyphs are 16x16 display pixels
    sdtx_canvas(sapp_width()/2, sapp_height()/2);
    sdtx_home();
    sdtx_color3f(1.0f, 1.0f, 0.0f);
    sdtx_puts("Hello World!\n");
    sdtx_color3b(0, 255, 255);
    sdtx_puts("1234567890!");

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
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
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 640,
        .height = 480,
        .gl_force_gles2 = true,
        .window_title = "debugtext-sapp",
    };
}
