//------------------------------------------------------------------------------
//  icon-sapp.c
//
//  Test/demo for setting the window icon.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_IMPL
#include "sokol_debugtext.h"

#define ICON_WIDTH (8)
#define ICON_HEIGHT (8)

typedef enum {
    ICONMODE_NONE,
    ICONMODE_DEFAULT,
    ICONMODE_USER,

    NUM_ICONMODES,
} iconmode_t;

static const char* help_text[NUM_ICONMODES] = {
    "<NONE>",
    "1: default icon\n\n",
    "2: user icon\n\n",
};

static struct {
    bool icon_mode_changed;
    iconmode_t icon_mode;
    sg_pass_action pass_action;
} state = {
    .icon_mode_changed = false,
    .icon_mode = ICONMODE_NONE,
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.25f, 0.5f, 1.0f } }
    }
};

static void init(void) {
    // setup sokol-gfx and sokol-debugtext
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric()
    });
}

// forward declared helper functions to set user icon
static void set_user_icon(void);

static void frame(void) {
    // apply static icon mode changes
    if (state.icon_mode_changed) {
        state.icon_mode_changed = false;
        switch (state.icon_mode) {
            case ICONMODE_DEFAULT:
                sapp_set_icon(&(sapp_icon_desc){ .sokol_default = true });
                break;
            case ICONMODE_USER:
                set_user_icon();
                break;
            default: break;
        }
    }

    // print help text
    sdtx_canvas(sapp_widthf()*0.5f, sapp_heightf()*0.5f);
    sdtx_origin(1.0f, 2.0f);
    sdtx_home();
    sdtx_puts("Press key to switch icon:\n\n\n");
    for (int i = 0; i < NUM_ICONMODES; i++) {
        if (i != ICONMODE_NONE) {
            sdtx_puts((i == (int)state.icon_mode) ? "==> " : "    ");
            sdtx_puts(help_text[i]);
        }
    }

    // just clear the window client area to some color
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_CHAR) {
        switch (ev->char_code) {
            case '1': state.icon_mode_changed = true; state.icon_mode = ICONMODE_DEFAULT; break;
            case '2': state.icon_mode_changed = true; state.icon_mode = ICONMODE_USER; break;
            default: break;
        }
    }
}

static void cleanup(void) {
    sdtx_shutdown();
    sg_shutdown();
}

// helper functions for setting up user image icons
static void fill_arrow_pixels(uint32_t* pixels, uint32_t w, uint32_t h) {
    static uint32_t colors[4] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF }; // red, green, blue, yellow
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t color = colors[((x ^ y)>>1) & 3];  // RGBY checker pattern
            // arrow shape
            if (y < (h/2)) {
                if ((x < (h/2-y)) || (x > (h/2+y))) {
                    color = 0;
                }
            }
            else {
                if ((x < w/4) || (x > (w/4)*3)) {
                    color = 0;
                }
            }
            pixels[y * h + x] = color;
        }
    }
}

static void set_user_icon(void) {
    // create 3 icon image candidates, 16x16, 32x32 and 64x64 pixels
    // the sokol-app backend code will pick the best match by size
    uint32_t small[16 * 16];
    uint32_t medium[32 * 32];
    uint32_t big[64 * 64];

    fill_arrow_pixels(small, 16, 16);
    fill_arrow_pixels(medium, 32, 32);
    fill_arrow_pixels(big, 64, 64);

    sapp_set_icon(&(sapp_icon_desc){
        .images = {
            { .width = 16, .height = 16, .pixels = SAPP_RANGE(small) },
            { .width = 32, .height = 32, .pixels = SAPP_RANGE(medium) },
            { .width = 64, .height = 64, .pixels = SAPP_RANGE(big) }
        }
    });
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "Window Icon Test",
    };
}
