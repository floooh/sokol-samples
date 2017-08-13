//------------------------------------------------------------------------------
//  clear-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"

sg_pass_action pass_action;

void init(const void* mtl_device) {
    /* setup sokol */
    sg_desc desc = {
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    };
    sg_setup(&desc);

    /* setup pass action to clear to red */
    pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };
}

void frame() {
    /* animate clear colors */
    float g = pass_action.colors[0].val[1] + 0.01f;
    if (g > 1.0f) g = 0.0f;
    pass_action.colors[0].val[1] = g;

    /* draw one frame */
    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 1, "Sokol Clear (Metal)", init, frame, shutdown);
    return 0;
}
