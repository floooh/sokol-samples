//------------------------------------------------------------------------------
//  cube-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"

const int msaa_sample_count = 4;
sg_pass_action pass_action = {0};

void init(const void* mtl_device) {
    /* setup sokol */
    sg_desc desc = {
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    };
    sg_setup(&desc);
}

void frame() {
    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, msaa_sample_count, "Sokol Cube (Metal)", init, frame, shutdown);
    return 0;
}
