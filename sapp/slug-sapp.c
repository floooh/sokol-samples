//------------------------------------------------------------------------------
//  slug-sapp.c
//
//  Demonstrates Slug text rendering. Ported from sokol-slug-odin:
//  https://tangled.org/dosha.dev/sokol-slug-odin/
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "util/fileutil.h"
#include "slugutil/slugutil.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define MAX_FONTS (3)
#define MAX_FONT_SIZE (2 * 1024 * 1024)

static struct {
    sg_pass_action pass_action;
    struct {
        slug_font_t cairo;
        slug_font_t lucide;
        slug_font_t twemoji;
    } fonts;
} state;

uint8_t file_buffers[MAX_FONTS][MAX_FONT_SIZE];

static void cairo_callback(const sfetch_response_t* response);
static void lucide_callback(const sfetch_response_t* response);
static void twemoji_callback(const sfetch_response_t* response);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 1,
        .num_lanes = 3,
        .max_requests = 3,
        .logger.func = slog_func,
    });

    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.1f, 0.1f, 0.1f, 1.0f } },
    };

    // start loading fonts
    char buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("Cairo.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[0], .size = sizeof(file_buffers[0]) },
        .callback = cairo_callback,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("lucide.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[1], .size = sizeof(file_buffers[1]) },
        .callback = lucide_callback,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("twemoji.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[2], .size = sizeof(file_buffers[2]) },
        .callback = twemoji_callback,
    });
}

static void frame(void) {
    sfetch_dowork();
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    if (state.fonts.cairo.valid) {
        // FIXME: draw cairo characters
    }
    if (state.fonts.lucide.valid) {
        // FIXME: draw lucide characters
    }
    if (state.fonts.twemoji.valid) {
        // FIXME: draw twemoji characters
    }
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    // FIXME
    (void)ev;
}

static void cleanup(void) {
    slug_unload_font(&state.fonts.cairo);
    slug_unload_font(&state.fonts.lucide);
    slug_unload_font(&state.fonts.twemoji);
    sfetch_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

static void cairo_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.cairo, 48.0f, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static void lucide_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.lucide, 48.0f, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static void twemoji_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.twemoji, 48.0f, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
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
        .sample_count = 1,
        .high_dpi = true,
        .window_title = "slug-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
