//------------------------------------------------------------------------------
//  clear-raspi.c
//  Simple draw loop, clear default framebuffer.
//------------------------------------------------------------------------------
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "raspientry.h"
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"

sg_pass_action pass_action;
int main() {
    raspi_init(1);
    sg_setup(&(sg_desc){0});

    pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    while (raspi_process_events()) {
        float g = pass_action.colors[0].val[1] + 0.01f;
        if (g > 1.0f) g = 0.0f;
        pass_action.colors[0].val[1] = g;
        sg_begin_default_pass(&pass_action, raspi_width(), raspi_height());
        sg_end_pass();
        sg_commit();
    }
    sg_shutdown();
    raspi_shutdown();
    return 0;
}
