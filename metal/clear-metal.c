//------------------------------------------------------------------------------
//  clear-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"

static sg_pass_action pass_action;

static void init(void) {
    sg_setup(&(sg_desc) {
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    // setup pass action to clear to red
    pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };
}

static void frame(void) {
    // animate clear colors
    float g = pass_action.colors[0].clear_value.g + 0.01f;
    if (g > 1.0f) g = 0.0f;
    pass_action.colors[0].clear_value.g = g;

    // draw one frame
    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = osx_swapchain() });
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 1, SG_PIXELFORMAT_NONE, "clear-metal", init, frame, shutdown);
    return 0;
}
