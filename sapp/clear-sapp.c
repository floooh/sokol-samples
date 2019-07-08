//------------------------------------------------------------------------------
//  clear-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "dbgui/dbgui.h"

static sg_pass_action pass_action;

void init(void) {
    sg_desc desc = { };
    desc.gl_force_gles2 = true,
    desc.mtl_device = sapp_metal_get_device();
    desc.mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor;
    desc.mtl_drawable_cb = sapp_metal_get_drawable;
    desc.d3d11_device = sapp_d3d11_get_device();
    desc.d3d11_device_context = sapp_d3d11_get_device_context();
    desc.d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view;
    desc.d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view;
    desc.gl_force_gles2 = sapp_gles2();
    sg_setup(&desc);

    // initial clear color
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].val[0] = 1.0f;
    pass_action.colors[0].val[1] = 0.0f;
    pass_action.colors[0].val[2] = 0.0f;
    pass_action.colors[0].val[3] = 1.0f;

    __dbgui_setup(1);
}

void frame(void) {
    float g = pass_action.colors[0].val[1] + 0.01f;
    pass_action.colors[0].val[1] = (g > 1.0f) ? 0.0f : g;
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = __dbgui_event;
    desc.width = 400;
    desc.height = 300;
    desc.gl_force_gles2 = true;
    desc.window_title = "Clear (sokol-app)";
    return desc;
}

