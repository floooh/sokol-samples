//------------------------------------------------------------------------------
//  triangle-sgl-sapp.c
//  Hello Triangle via sokol_gl.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "ui/dbgui.h"

static sg_pass_action pass_action = {
    .colors[0] = {
        .action = SG_ACTION_CLEAR,
        .val = { 0.0f, 0.0f, 0.0f, 1.0f }
    }
};

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
    __dbgui_setup(1);
    /* setup sokol-gl with all-default settings */
    sgl_setup(&(sgl_desc_t){0});
}

void frame(void) {
    /* all sokol-gl functions except sgl_draw() can be called anywhere in the frame */
    static float a = 0.0f;
    a += 1.0f;
    sgl_load_identity();
    sgl_rotate(a, 0.0f, 0.0f, 1.0f);
    sgl_translate(0.5f, 0.0f, 0.0f);
    sgl_rotate(a, 0.0f, 0.0f, 1.0f);
    sgl_scale(0.5f, 0.5f, 0.5f);
    sgl_begin_triangles();
    sgl_v2f_c3b( -0.5f,  0.5f,  255, 0, 0);
    sgl_v2f_c3b(  0.5f,  0.5f,  0, 255, 0);
    sgl_v2f_c3b(  0.5f, -0.5f,  0, 0, 255);
    sgl_v2f_c3b( -0.5f,  0.5f,  255, 0, 0);
    sgl_v2f_c3b(  0.5f, -0.5f,  0, 0, 255);
    sgl_v2f_c3b( -0.5f, -0.5f,  255, 255, 0);
    sgl_end();

    /* Render the sokol-gfx default pass, all sokol-gl commands
       that happened so far are rendered inside sgl_draw(), and this
       is the only sokol-gl function that must be called inside
       a sokol-gfx begin/end pass pair.
       sgl_draw() also 'rewinds' sokol-gl for the next frame.
    */
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
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
        .width = 512,
        .height = 512,
        .gl_force_gles2 = true,
        .window_title = "Triangle (sokol-gl + sokol-app)",
    };
}
