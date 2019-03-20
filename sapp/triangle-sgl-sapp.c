//------------------------------------------------------------------------------
//  sgl-triangle-sapp.c
//  Hello Triangle via sokol_gl.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "ui/dbgui.h"

void init(void) {
    sg_setup(&(sg_desc){
        .gl_force_gles2 = true,
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });
    sgl_setup(&(sgl_desc_t){0});
    __dbgui_setup(1);
}

void frame(void) {
    sg_pass_action pass_action = {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .val = { 0.024f, 0.353f, 0.271f, 1.0f }
        }
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sgl_begin(SGL_TRIANGLES);
    /*              pos                  color */ 
    sgl_v3f_c4f( 0.0f,  0.5f, 0.5f,  1.0f, 0.0f, 0.0f, 1.0f);
    sgl_v3f_c4f( 0.5f, -0.5f, 0.5f,  0.0f, 1.0f, 0.0f, 1.0f);
    sgl_v3f_c4f(-0.5f, -0.5f, 0.5f,  0.0f, 0.0f, 1.0f, 1.0f);
    sgl_end();
    sgl_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "Sokol GL Triangle (sokol-app)",
    };
}
