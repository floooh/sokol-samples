//------------------------------------------------------------------------------
//  window-sapp.c
//
//  Test setting window properties like window title and window icon.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#define ICON_WIDTH (8)
#define ICON_HEIGHT (8)

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });

    const uint32_t X = 0xFF0000FF;
    const uint32_t o = 0x000000FF;
    const uint32_t pixels[ICON_HEIGHT][ICON_WIDTH] = {
        { X, X, X, X, X, X, X, X },
        { X, o, o, o, o, o, o, X },
        { X, o, o, o, o, o, o, X },
        { X, o, o, o, o, o, o, X },
        { X, o, o, o, o, o, o, X },
        { X, o, o, o, o, o, o, X },
        { X, o, o, o, o, o, o, X },
        { X, X, X, X, X, X, X, X },
    };

    sapp_set_window_icon(&(sapp_icon){
        .images[0] = {
            .width = ICON_WIDTH,
            .height = ICON_HEIGHT,
            .pixels = SAPP_RANGE(pixels)
        }
    });
}

static void frame(void) {
    const sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.5f, 0.0f, 1.0f } }
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "Window Attributes"
    };
}
