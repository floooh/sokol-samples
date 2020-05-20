//------------------------------------------------------------------------------
//  debugtext-sapp.c
//  Simple debug text rendering with sokol_debugtext.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#define SOKOL_DEBUGTEXT_ALL_FONTS
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"

static struct {
    sg_pass_action pass_action;
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.125f, 0.25f, 1.0f } }
    }
};

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-debugtext
    sdtx_setup(&(sdtx_desc_t){
        .context = {
            // FIXME: the need for a cast here sucks... (latest clang)
            .color_format = (sg_pixel_format) sapp_color_format(),
            .depth_format = (sg_pixel_format) sapp_depth_format(),
            .sample_count = sapp_sample_count()
        }
    });
}

static void print_font(sdtx_font_t font, const char* title, uint8_t r, uint8_t g, uint8_t b) {
    sdtx_font(font);
    sdtx_color3b(r, g, b);
    sdtx_puts(title);
    for (int c = 32; c < 256; c++) {
        sdtx_putc(c);
        if (((c + 1) & 63) == 0) {
            sdtx_crlf();
        }
    }
    sdtx_crlf();
}

static void frame(void) {

    // set virtual canvas size to half display size so that
    // glyphs are 16x16 display pixels
    sdtx_canvas(sapp_width()*0.5f, sapp_height()*0.5f);
    sdtx_origin(0.0f, 16.0f);
    sdtx_home();
    print_font(SDTX_FONT_KC853, "KC85/3:\n",      0xf4, 0x43, 0x36);
    print_font(SDTX_FONT_KC853, "KC85/4:\n",      0x21, 0x96, 0xf3);
    print_font(SDTX_FONT_Z1013, "Z1013:\n",       0x4c, 0xaf, 0x50);
    print_font(SDTX_FONT_CPC,   "Amstrad CPC:\n", 0xff, 0xeb, 0x3b);
    print_font(SDTX_FONT_C64,   "C64:\n",         0x79, 0x86, 0xcb);
    print_font(SDTX_FONT_ORIC,  "Oric Atmos:\n",  0xff, 0x98, 0x00);

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sdtx_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 1024,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "debugtext-sapp",
    };
}
