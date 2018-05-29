//------------------------------------------------------------------------------
//  clear-emsc.c
//  Simple draw loop, clear default framebuffer.
//------------------------------------------------------------------------------
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"
#include "emsc.h"

sg_pass_action pass_action;

void draw();

int main() {
    /* setup WebGL1 context, no antialiasing */
    emsc_init("#canvas", EMSC_NONE);

    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());

    /* setup pass action to clear to red */
    pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

void draw() {
    /* animate clear colors */
    float g = pass_action.colors[0].val[1] + 0.01f;
    if (g > 1.0f) g = 0.0f;
    pass_action.colors[0].val[1] = g;

    /* draw one frame */
    sg_begin_default_pass(&pass_action, emsc_width(), emsc_height());
    sg_end_pass();
    sg_commit();
}
