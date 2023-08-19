//------------------------------------------------------------------------------
//  clear-emsc.c
//  Simple draw loop, clear default framebuffer.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"

sg_pass_action pass_action;

static EM_BOOL draw(double time, void* userdata);

int main() {
    // setup WebGL1 context, no antialiasing
    emsc_init("#canvas", EMSC_NONE);

    // setup sokol_gfx
    sg_setup(&(sg_desc){ .logger.func = slog_func });
    assert(sg_isvalid());

    // setup pass action to clear to red
    pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    // animate clear colors
    float g = pass_action.colors[0].clear_value.g + 0.01f;
    if (g > 1.0f) g = 0.0f;
    pass_action.colors[0].clear_value.g = g;

    // draw one frame
    sg_begin_default_pass(&pass_action, emsc_width(), emsc_height());
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}
