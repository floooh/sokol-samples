//------------------------------------------------------------------------------
//  nuklear-images-sapp.c
//
//  Demonstrate/test using sg_image textures via nk_image in Nuklear UI.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"

// include nuklear.h before the sokol_nuklear.h implementation
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_IMPLEMENTATION
#include "nuklear/nuklear.h"
#define SOKOL_NUKLEAR_IMPL
#include "sokol_nuklear.h"

static struct {
    // NOTE: Nuklear image handles, not sg_image!
    struct nk_image img0;
    struct nk_image img1;
    sg_pass_action pass_action;
} state;

#define IMG_WIDTH (16)
#define IMG_HEIGHT (16)
#define BLACK (0xFF000000)
#define WHITE (0xFFFFFFFF)
#define BLUE (0xFFED5532)
#define YELLOW (0xFF03DAFF)

// helper function to create a sokol-gfx image wrapped in a Nuklear image handle
struct nk_image make_image_handle(const sg_image_desc* img_desc) {
    return nk_image_handle(snk_nkhandle(sg_make_image(img_desc)));
}

static void init(void) {
    // setup sokok-gfx, sokol_fetch and sokol-nuklear
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());
    snk_setup(&(snk_desc_t){
        .dpi_scale = sapp_dpi_scale(),
    });

    // create two simple images
    uint32_t pixels[IMG_WIDTH][IMG_HEIGHT] = { 0 };

    // black/white checkerboard
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            pixels[y][x] = ((x ^ y) & 1) ? BLACK : WHITE;
        }
    }
    state.img0 = make_image_handle(&(sg_image_desc){
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .data.subimage[0][0] = SG_RANGE(pixels)
    });

    // blue/yellow checkerboard
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            pixels[y][x] = ((x ^ y) & 1) ? BLUE : YELLOW;
        }
    }
    state.img1 = make_image_handle(&(sg_image_desc){
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .data.subimage[0][0] = SG_RANGE(pixels)
    });

    // pass action for clearing background
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.25f, 0.5f, 0.75f, 1.0f } }
    };
}

static void frame(void) {
    struct nk_context* ctx = snk_new_frame();

    // declare Nuklear UI
    if (nk_begin(ctx, "Sokol+Nuklear Image Test", nk_rect(10, 25, 460, 300), NK_WINDOW_BORDER|NK_WINDOW_SCALABLE|NK_WINDOW_MOVABLE|NK_WINDOW_MINIMIZABLE)) {
        nk_layout_row_static(ctx, 192, 192, 2);
        nk_image(ctx, state.img0);
        nk_image(ctx, state.img1);
    }
    nk_end(ctx);

    // draw sokol-gfx frame
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    snk_render(sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (__dbgui_event_with_retval(ev)) {
        return;
    }
    snk_handle_event(ev);
}

static void cleanup(void) {
    snk_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .enable_clipboard = true,
        .width = 800,
        .height = 600,
        .window_title = "nuklear-image-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
