//------------------------------------------------------------------------------
//  sgl-sapp.c
//  Rendering via sokol_gl.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "ui/dbgui.h"

#define SAMPLE_COUNT (4)

static sg_pass_action pass_action = {
    .colors[0] = {
        .action = SG_ACTION_CLEAR,
        .val = { 0.0f, 0.0f, 0.0f, 1.0f }
    }
};
sg_image img;

static void init(void) {
    sg_setup(&(sg_desc){
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view,
    });
    __dbgui_setup(SAMPLE_COUNT);
    /* setup sokol-gl with all-default settings */
    sgl_setup(&(sgl_desc_t){
        .sample_count = SAMPLE_COUNT
    });

    /* a checkerboard texture */
    uint32_t pixels[8][8];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            pixels[y][x] = ((y ^ x) & 1) ? 0xFFFFFFFF : 0xFF000000;
        }
    }
    img = sg_make_image(&(sg_image_desc){
        .width = 8,
        .height = 8,
        .content.subimage[0][0] = {
            .ptr = pixels,
            .size = sizeof(pixels)
        }
    });
}

static void draw_triangle(void) {
    sgl_default_state();
    sgl_begin_triangles();
    sgl_v2f_c3b( 0.0f,  0.5f, 255, 0, 0);
    sgl_v2f_c3b(-0.5f, -0.5f, 0, 0, 255);
    sgl_v2f_c3b( 0.5f, -0.5f, 0, 255, 0);
    sgl_end();
}

static void draw_quad(void) {
    static float angle_deg = 0.0f;
    float scale = 1.0f + sinf(sgl_rad(angle_deg)) * 0.5f;
    angle_deg += 1.0f;
    sgl_default_state();
    sgl_rotate(sgl_rad(angle_deg), 0.0f, 0.0f, 1.0f);
    sgl_scale(scale, scale, 1.0f);
    sgl_begin_quads();
    sgl_v2f_c3b( -0.5f, -0.5f,  255, 255, 0);
    sgl_v2f_c3b(  0.5f, -0.5f,  0, 255, 0);
    sgl_v2f_c3b(  0.5f,  0.5f,  0, 0, 255);
    sgl_v2f_c3b( -0.5f,  0.5f,  255, 0, 0);
    sgl_end();
}

/* vertex specification for a cube with colored sides and texture coords */
static void cube(void) {
    sgl_begin_quads();
    sgl_c3f(1.0f, 0.0f, 0.0f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f, -1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f, -1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(0.0f, 1.0f, 0.0f);
        sgl_v3f_t2f(-1.0, -1.0,  1.0, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0, -1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0,  1.0,  1.0,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0,  1.0,  1.0, -1.0f, -1.0f);
    sgl_c3f(0.0f, 0.0f, 1.0f);
        sgl_v3f_t2f(-1.0, -1.0,  1.0, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0,  1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0,  1.0, -1.0,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0, -1.0, -1.0, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.5f, 0.0f);
        sgl_v3f_t2f(1.0, -1.0,  1.0, -1.0f,   1.0f);
        sgl_v3f_t2f(1.0, -1.0, -1.0,  1.0f,   1.0f);
        sgl_v3f_t2f(1.0,  1.0, -1.0,  1.0f,  -1.0f);
        sgl_v3f_t2f(1.0,  1.0,  1.0, -1.0f,  -1.0f);
    sgl_c3f(0.0f, 0.5f, 1.0f);
        sgl_v3f_t2f( 1.0, -1.0, -1.0, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0, -1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0, -1.0,  1.0,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0, -1.0, -1.0, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.0f, 0.5f);
        sgl_v3f_t2f(-1.0,  1.0, -1.0, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0,  1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0,  1.0,  1.0,  1.0f, -1.0f);
        sgl_v3f_t2f( 1.0,  1.0, -1.0, -1.0f, -1.0f);
    sgl_end();
}

static void draw_cubes(void) {
    static float rot[2] = { 0.0f, 0.0f };
    rot[0] += 1.0f;
    rot[1] += 2.0f;

    sgl_default_state();
    sgl_enable_depth_test();
    sgl_enable_cull_face();

    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(45.0f), 1.0f, 0.1f, 100.0f);

    sgl_matrix_mode_modelview();
    sgl_translate(0.0f, 0.0f, -12.0f);
    sgl_rotate(sgl_rad(rot[0]), 1.0f, 0.0f, 0.0f);
    sgl_rotate(sgl_rad(rot[1]), 0.0f, 1.0f, 0.0f);
    cube();
    sgl_push_matrix();
        sgl_translate(0.0f, 0.0f, 3.0f);
        sgl_scale(0.5f, 0.5f, 0.5f);
        sgl_rotate(-2.0f * sgl_rad(rot[0]), 1.0f, 0.0f, 0.0f);
        sgl_rotate(-2.0f * sgl_rad(rot[1]), 0.0f, 1.0f, 0.0f);
        cube();
        sgl_push_matrix();
            sgl_translate(0.0f, 0.0f, 3.0f);
            sgl_scale(0.5f, 0.5f, 0.5f);
            sgl_rotate(-3.0f * sgl_rad(2*rot[0]), 1.0f, 0.0f, 0.0f);
            sgl_rotate(3.0f * sgl_rad(2*rot[1]), 0.0f, 0.0f, 1.0f);
            cube();
        sgl_pop_matrix();
    sgl_pop_matrix();
}

static void draw_tex_cube(void) {
    static float rot[2] = { 0.0f, 0.0f };
    rot[0] += 0.1f;
    rot[1] += 0.2f;
    const float s = 1.0f + sinf(rot[0]) * 0.5f;

    sgl_default_state();
    sgl_enable_depth_test();
    sgl_enable_cull_face();
    sgl_enable_texture();
    sgl_texture(img);

    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(45.0f), 1.0f, 0.1f, 100.0f);
    sgl_matrix_mode_modelview();
    sgl_translate(0.0f, 0.0f, -6.0f);
    sgl_rotate(sgl_rad(rot[0]), 1.0f, 0.0f, 0.0f);
    sgl_rotate(sgl_rad(rot[1]), 0.0f, 1.0f, 0.0f);
    sgl_matrix_mode_texture();
    sgl_rotate(5.0f * sgl_rad(rot[0]), 0.0f, 0.0f, 1.0f);
    sgl_scale(s, s, 1.0f);
    cube();
}

static void frame(void) {
    /* all sokol-gl functions except sgl_draw() can be called anywhere in the frame */
    const int w = sapp_width();
    const int h = sapp_height();
    const int wh = w/2;
    const int hh = h/2;
    sgl_viewport(0, 0, wh, hh, true);
    draw_triangle();
    sgl_viewport(wh, 0, wh, hh, true);
    draw_quad();
    sgl_viewport(0, hh, wh, hh, true);
    draw_cubes();
    sgl_viewport(wh, hh, wh, hh, true);
    draw_tex_cube();
    sgl_viewport(0, 0, w, h, true);

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

static void cleanup(void) {
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
        .sample_count = SAMPLE_COUNT,
        .gl_force_gles2 = true,
        .window_title = "sokol_gl.h (sokol-app)",
    };
}
