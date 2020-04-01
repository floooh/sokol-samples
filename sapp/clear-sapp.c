//------------------------------------------------------------------------------
//  clear-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "dbgui/dbgui.h"

static struct {
    sg_pass_action pass_action;
} state;

void init(void) {
    sg_setup(&(sg_desc){
        .context = {
            .gl = {
                .force_gles2 = sapp_gles2()
            },
            .metal = {
                .device = sapp_metal_get_device(),
                .renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
                .drawable_cb = sapp_metal_get_drawable
            },
            .d3d11 = {
                .device = sapp_d3d11_get_device(),
                .device_context = sapp_d3d11_get_device_context(),
                .render_target_view_cb = sapp_d3d11_get_render_target_view,
                .depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
            },
            .wgpu = {
                .device = sapp_wgpu_get_device(),
                .render_format = sapp_wgpu_get_render_format(),
                .render_view_cb = sapp_wgpu_get_render_view,
                .resolve_view_cb = sapp_wgpu_get_resolve_view,
                .depth_stencil_view_cb = sapp_wgpu_get_depth_stencil_view
            }
        }
    });
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action=SG_ACTION_CLEAR, .val={1.0f, 0.0f, 0.0f, 1.0f} }
    };
    __dbgui_setup(1);
}

void frame(void) {
    float g = state.pass_action.colors[0].val[1] + 0.01f;
    state.pass_action.colors[0].val[1] = (g > 1.0f) ? 0.0f : g;
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 400,
        .height = 300,
        .gl_force_gles2 = true,
        .window_title = "Clear (sokol app)",
    };
}

