//------------------------------------------------------------------------------
//  sgl-sapp.c
//  Rendering via sokol_gl.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "dbgui/dbgui.h"

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sgl_pipeline pip_3d;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    /* setup sokol-gl */
    sgl_setup(&(sgl_desc_t){
        .sample_count = sapp_sample_count()
    });

    /* a checkerboard texture */
    uint32_t pixels[8][8];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            pixels[y][x] = ((y ^ x) & 1) ? 0xFFFFFFFF : 0xFF000000;
        }
    }
    state.img = sg_make_image(&(sg_image_desc){
        .width = 8,
        .height = 8,
        .content.subimage[0][0] = {
            .ptr = pixels,
            .size = sizeof(pixels)
        }
    });

    /* create a pipeline object for 3d rendering, with less-equal
       depth-test and cull-face enabled, not that we don't provide
       a shader, vertex-layout, pixel formats and sample count here,
       these are all filled in by sokol-gl
    */
    state.pip_3d = sgl_make_pipeline(&(sg_pipeline_desc){
        .depth_stencil = {
            .depth_write_enabled = true,
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK
        }
    });

    /* default pass action */
    state.pass_action = (sg_pass_action) {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .val = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };
}

static void draw_triangle(void) {
    sgl_defaults();
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
    sgl_defaults();
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

    sgl_defaults();
    sgl_load_pipeline(state.pip_3d);

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
    static float frame_count = 0.0f;
    frame_count += 1.0f;
    float a = sgl_rad(frame_count);

    // texture matrix rotation and scale
    float tex_rot = 0.5f * a;
    const float tex_scale = 1.0f + sinf(a) * 0.5f;

    // compute an orbiting eye-position for testing sgl_lookat()
    float eye_x = sinf(a) * 6.0f;
    float eye_z = cosf(a) * 6.0f;
    float eye_y = sinf(a) * 3.0f;

    sgl_defaults();
    sgl_load_pipeline(state.pip_3d);

    sgl_enable_texture();
    sgl_texture(state.img);

    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(45.0f), 1.0f, 0.1f, 100.0f);
    sgl_matrix_mode_modelview();
    sgl_lookat(eye_x, eye_y, eye_z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    sgl_matrix_mode_texture();
    sgl_rotate(tex_rot, 0.0f, 0.0f, 1.0f);
    sgl_scale(tex_scale, tex_scale, 1.0f);
    cube();
}

static void frame(void) {
    /* compute viewport rectangles so that the views are horizontally
       centered and keep a 1:1 aspect ratio
    */
    const int dw = sapp_width();
    const int dh = sapp_height();
    const int ww = dh/2; /* not a bug */
    const int hh = dh/2;
    const int x0 = dw/2 - hh;
    const int x1 = dw/2;
    const int y0 = 0;
    const int y1 = dh/2;
    /* all sokol-gl functions except sgl_draw() can be called anywhere in the frame */
    sgl_viewport(x0, y0, ww, hh, true);
    draw_triangle();
    sgl_viewport(x1, y0, ww, hh, true);
    draw_quad();
    sgl_viewport(x0, y1, ww, hh, true);
    draw_cubes();
    sgl_viewport(x1, y1, ww, hh, true);
    draw_tex_cube();
    sgl_viewport(0, 0, dw, dh, true);

    /* Render the sokol-gfx default pass, all sokol-gl commands
       that happened so far are rendered inside sgl_draw(), and this
       is the only sokol-gl function that must be called inside
       a sokol-gfx begin/end pass pair.
       sgl_draw() also 'rewinds' sokol-gl for the next frame.
    */
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
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
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 512,
        .height = 512,
        .sample_count = 4,
        .gl_force_gles2 = true,
        .window_title = "sokol_gl.h (sokol-app)",
    };
}
