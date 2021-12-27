//------------------------------------------------------------------------------
//  sgl-context-sapp.c
//
//  Demonstrates how to render in different render passes with sokol_gl.h
//  using sokol-gl contexts.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "dbgui/dbgui.h"

static struct {
    struct {
        sg_pass_action pass_action;
        sg_pass pass;
        sg_image img;
        sgl_context sgl_ctx;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sgl_pipeline sgl_pip;
    } display;
    float frame_count;
} state;

#define OFFSCREEN_PIXELFORMAT (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_SAMPLECOUNT (1)
#define OFFSCREEN_WIDTH (32)
#define OFFSCREEN_HEIGHT (32)

// helper functions (at the end of this file)
static void draw_cube(void);
static void draw_quad(void);

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-gl with the default context compatible with the default render pass
    sgl_setup(&(sgl_desc_t){
        .max_vertices = 64,
        .max_commands = 16,
    });

    // pass action and pipeline for the default render pass
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.5f, 0.7f, 1.0f, 1.0f } }
    };
    state.display.sgl_pip = sgl_context_make_pipeline(sgl_default_context(), &(sg_pipeline_desc){
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL
        }
    });

    // create a sokol-gl context compatible with the offscreen render pass
    // (specific color pixel format, no depth-stencil-surface, no MSAA)
    state.offscreen.sgl_ctx = sgl_make_context(&(sgl_context_desc_t){
        .max_vertices = 8,
        .max_commands = 4,
        .color_format = OFFSCREEN_PIXELFORMAT,
        .depth_format = SG_PIXELFORMAT_NONE,
        .sample_count = OFFSCREEN_SAMPLECOUNT,
    });

    // create an offscreen render target texture, pass, and pass_action
    state.offscreen.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_PIXELFORMAT,
        .sample_count = OFFSCREEN_SAMPLECOUNT,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST
    });
    state.offscreen.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = state.offscreen.img
    });
    state.offscreen.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    const float a = sgl_rad((float)sapp_frame_count() * t);

    // draw a rotating quad into the offscreen render target texture
    sgl_set_context(state.offscreen.sgl_ctx);
    sgl_defaults();
    sgl_matrix_mode_modelview();
    sgl_rotate(a, 0.0f, 0.0f, 1.0f);
    draw_quad();

    // draw a rotating 3D cube, using the offscreen render target as texture
    sgl_set_context(SGL_DEFAULT_CONTEXT);
    sgl_defaults();
    sgl_enable_texture();
    sgl_texture(state.offscreen.img);
    sgl_load_pipeline(state.display.sgl_pip);
    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(45.0f), sapp_widthf()/sapp_heightf(), 0.1f, 100.0f);
    const float eye[3] = { sinf(a) * 6.0f, sinf(a) * 3.0f, cosf(a) * 6.0f };
    sgl_matrix_mode_modelview();
    sgl_lookat(eye[0], eye[1], eye[2], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    draw_cube();

    // do the actual offscreen and display rendering in sokol-gfx passes
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sgl_context_draw(state.offscreen.sgl_ctx);
    sg_end_pass();
    sg_begin_default_pass(&state.display.pass_action, sapp_width(), sapp_height());
    sgl_context_draw(SGL_DEFAULT_CONTEXT);
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
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .gl_force_gles2 = true,
        .window_title = "sokol-gl contexts (sapp)",
        .icon.sokol_default = true,
    };
}

// helper function to draw a colored quad with sokol-gl
static void draw_quad(void) {
    sgl_begin_quads();
    sgl_v2f_c3b( 0.0f, -1.0f, 255, 0, 0);
    sgl_v2f_c3b( 1.0f,  0.0f, 0, 0, 255);
    sgl_v2f_c3b( 0.0f,  1.0f, 0, 255, 255);
    sgl_v2f_c3b(-1.0f,  0.0f, 0, 255, 0);
    sgl_end();
}

// helper function to draw a textured cube with sokol-gl
static void draw_cube(void) {
    sgl_begin_quads();
    sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, 0.0f, 1.0f);
    sgl_v3f_t2f( 1.0f,  1.0f, -1.0f, 1.0f, 1.0f);
    sgl_v3f_t2f( 1.0f, -1.0f, -1.0f, 1.0f, 0.0f);
    sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f);
    sgl_v3f_t2f(-1.0f, -1.0f,  1.0f, 0.0f, 1.0f);
    sgl_v3f_t2f( 1.0f, -1.0f,  1.0f, 1.0f, 1.0f);
    sgl_v3f_t2f( 1.0f,  1.0f,  1.0f, 1.0f, 0.0f);
    sgl_v3f_t2f(-1.0f,  1.0f,  1.0f, 0.0f, 0.0f);
    sgl_v3f_t2f(-1.0f, -1.0f,  1.0f, 0.0f, 1.0f);
    sgl_v3f_t2f(-1.0f,  1.0f,  1.0f, 1.0f, 1.0f);
    sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, 1.0f, 0.0f);
    sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f);
    sgl_v3f_t2f( 1.0f, -1.0f,  1.0f, 0.0f, 1.0f);
    sgl_v3f_t2f( 1.0f, -1.0f, -1.0f, 1.0f, 1.0f);
    sgl_v3f_t2f( 1.0f,  1.0f, -1.0f, 1.0f, 0.0f);
    sgl_v3f_t2f( 1.0f,  1.0f,  1.0f, 0.0f, 0.0f);
    sgl_v3f_t2f( 1.0f, -1.0f, -1.0f, 0.0f, 1.0f);
    sgl_v3f_t2f( 1.0f, -1.0f,  1.0f, 1.0f, 1.0f);
    sgl_v3f_t2f(-1.0f, -1.0f,  1.0f, 1.0f, 0.0f);
    sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f);
    sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, 0.0f, 1.0f);
    sgl_v3f_t2f(-1.0f,  1.0f,  1.0f, 1.0f, 1.0f);
    sgl_v3f_t2f( 1.0f,  1.0f,  1.0f, 1.0f, 0.0f);
    sgl_v3f_t2f( 1.0f,  1.0f, -1.0f, 0.0f, 0.0f);
    sgl_end();
}
