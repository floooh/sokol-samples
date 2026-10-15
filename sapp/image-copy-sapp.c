//------------------------------------------------------------------------------
//  image-copy-sapp.c
//
//  Demonstrates sg_copy_image_to_image(). Renders a rotating cube into
//  a small offscreen render target and takes snapshots via
//  sg_copy_image_to_image into a bigger render target which is then
//  rendered as quad to the display.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_LETTERBOX_IMPL
#include "sokol_letterbox.h"
#include "dbgui/dbgui.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "image-copy-sapp.glsl.h"

#define SMALL_RENDER_SIZE (64)
#define BIG_RENDER_SIZE (SMALL_RENDER_SIZE * 8)

static struct {
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
    } cube;
    struct {
        sg_image color_img;
        sg_image depth_img;
        sg_pass pass;
    } offscreen;
    struct {
        sg_image img;
        sg_view tex_view;
        sg_pass_action pass_action;
    } display;
} state;

// cube vertices and indices
static const float vertices[] = {
    -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
     1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
     1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
    -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

    -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
     1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
     1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
    -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

    -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

    1.0, -1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
    1.0,  1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
    1.0,  1.0,  1.0,    1.0, 0.5, 0.0, 1.0,
    1.0, -1.0,  1.0,    1.0, 0.5, 0.0, 1.0,

    -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
    -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
     1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
     1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

    -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
    -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
     1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
     1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
};

static const uint16_t cube_indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    _dbgui_setup();

    // create a small render target and depth-buffer image as copy-src
    state.offscreen.color_img = sg_make_image(&(sg_image_desc){
        .usage = {
            .color_attachment = true,
            .copy_src = true,
        },
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .width = SMALL_RENDER_SIZE,
        .height = SMALL_RENDER_SIZE,
        .sample_count = 1,
        .label = "copy-src-image",
    });
    state.offscreen.depth_img = sg_make_image(&(sg_image_desc){
        .usage = {
            .depth_stencil_attachment = true,
        },
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .width = SMALL_RENDER_SIZE,
        .height = SMALL_RENDER_SIZE,
        .label = "dpeth-buffer",
    });

    // and the pass struct (pass-attachments and pass-action)
    state.offscreen.pass = (sg_pass){
        .attachments = {
            .colors[0] = sg_make_view(&(sg_view_desc){
                .color_attachment.image = state.offscreen.color_img,
            }),
            .depth_stencil = sg_make_view(&(sg_view_desc){
                .depth_stencil_attachment.image = state.offscreen.depth_img,
            }),
        },
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
            },
        }
    };

    // ...and a large image as copy-dst and its texture view
    state.display.img = sg_make_image(&(sg_image_desc){
        .usage = {
            .copy_src = true,
        },
        .width = BIG_RENDER_SIZE,
        .height = BIG_RENDER_SIZE,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
    });
    state.display.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = state.display.img,
    });

    // the display pass-action (clear to black)
    state.display.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    // FIXME FIXME FIXME
}

static void frame(void) {
    // FIXME
    _dbgui_update();
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain()
    });
    _dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    _dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = _dbgui_event,
        .width = 512,
        .height = 512,
        .window_title = "image-copy-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
