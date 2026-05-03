//------------------------------------------------------------------------------
//  ilbm-sapp.c
//  IFF ILBM loader, demonstrates sokol_framebuffer.h and sokol_letterbox.h.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#define SOKOL_FRAMEBUFFER_IMPL
#include "sokol_framebuffer.h"
#define SOKOL_LETTERBOX_IMPL
#include "sokol_letterbox.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#define SOKOL_APP_IMGUI_IMPL
#include "sokol_app_imgui.h"
#include "ilbm/ilbm.h"
#include "util/fileutil.h"

#define FB_WIDTH (320)
#define FB_HEIGHT (200)

#define MAX_FILE_SIZE (128 * 1024)
#define NUM_FILES (10)
static const char* files[NUM_FILES] = {
    "venus.iff",
    "celtic_woman.iff",
    "eye.iff",
    "eiffel_tower.iff",
    "kingtut.iff",
    "gorilla.iff",
    "paintcan.iff",
    "space.iff",
    "waterfall.iff",
    "yacht.iff",
};
static const char* artists[NUM_FILES] = {
    "Avril Harrison (1985)", // venus.iff
    "Avril Harrison (1988)", // celtic_woman.iff
    "Avril Harrison (1986)", // eye.iff
    "Avril Harrison (1986)", // eiffel_tower.iff
    "Avril Harrison (1985)", // kingtut.iff
    "Greg Johnson (1985)", // gorilla.iff
    "Greg Johnson (1985)", // paintcan.iff
    "Unknown (1989)", // space.iff
    "Unknown (1989)", // waterfall.iff
    "Unknown (1989)", // yacht.iff
};

// some of the images have active CRNG chunks even though
// no color cycling is intended for these images
static bool allow_color_cyling[NUM_FILES] = {
    false, // venus.iff
    false, // celtic_woman.iff
    false, // eye.iff
    false, // eiffel_tower.iff
    false, // kingtut.iff
    false, // gorilla.iff
    false, // paintcan.iff
    true, // space.iff
    true, // waterfall.iff
    true, // yacht.iff
};

static struct {
    sg_pass_action pass_action;
    sfb_framebuffer fb;
    ilbm_t ilbm;
    struct {
        bool pending;
        bool failed;
    } load;
    struct {
        int selected;
        bool allow_color_cycling;
    } ui;
} state;

static uint8_t file_buffer[MAX_FILE_SIZE];

static void draw_ui(void);
static void fetch_async(const char*);
static void fetch_callback(const sfetch_response_t*);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 1,
        .num_lanes = 1,
        .max_requests = 1,
        .logger.func = slog_func,
    });
    sgimgui_setup(&(sgimgui_desc_t){0});
    sappimgui_setup();
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sfb_setup(&(sfb_desc){
        .logger.func = slog_func,
    });

    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
        },
    };

    // start loading the first IFF image
    fetch_async(files[0]);
}

static void frame(void) {
    sfetch_dowork();
    draw_ui();
    const bool fb_valid = sfb_query_framebuffer_state(state.fb) == SFB_RESOURCESTATE_VALID;

    if (fb_valid && state.ui.allow_color_cycling && ilbm_color_cycle(&state.ilbm, sapp_frame_duration())) {
        // color palette needs to be updated
        sfb_update(state.fb, &(sfb_update_desc){
            .pixels = { .ptr = state.ilbm.pixels.ptr, .size = state.ilbm.pixels.size },
            .palette = SG_RANGE(state.ilbm.colors),
        });
    }

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    if (fb_valid) {
        float aspect_w = (float)(state.ilbm.x_aspect * state.ilbm.width);
        float aspect_h = (float)(state.ilbm.y_aspect * state.ilbm.height);
        const slbx_viewport vp = slbx_letterbox(sapp_width(), sapp_height(), &(slbx_letterbox_desc){
            .content_aspect_ratio = aspect_w / aspect_h,
            .border = { .top = 26, .left = 10, .right = 10, .bottom = 10 },
        });
        sg_apply_viewport(vp.x, vp.y, vp.width, vp.height, true);
        sfb_render(state.fb);
        sg_apply_viewport(0, 0, sapp_width(), sapp_height(), true);
    }
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    ilbm_free(&state.ilbm);
    sfb_shutdown();
    sappimgui_shutdown();
    sgimgui_shutdown();
    simgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    sappimgui_track_event(ev);
    simgui_handle_event(ev);
}

static void draw_ui(void) {
    sappimgui_track_frame();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        sappimgui_draw_menu("sokol-app");
        igEndMainMenuBar();
    }
    sgimgui_draw();
    sappimgui_draw();
    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        if (state.load.pending) {
            igText("Loading...");
        } else {
            if (state.load.failed) {
                igText("Loading failed!");
            }
            if (igComboChar("Image", &state.ui.selected, files, IM_ARRAYSIZE(files))) {
                state.ui.allow_color_cycling = allow_color_cyling[state.ui.selected];
                fetch_async(files[state.ui.selected]);
            }
            igText("Artist: %s", artists[state.ui.selected]);
            igText("Width: %d", state.ilbm.width);
            igText("Height: %d", state.ilbm.height);
            igText("Colors: %d", state.ilbm.num_colors);
            igCheckbox("Allow Color Cycling", &state.ui.allow_color_cycling);
        }
    }
    igEnd();
}

static void fetch_async(const char* filename) {
    state.load.pending = true;
    state.load.failed = false;
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path(filename, path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(file_buffer),
    });
}

static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load.pending = false;
        // NOTE: it's ok to call destroy functions with invalid handle
        sfb_destroy_framebuffer(state.fb);
        ilbm_free(&state.ilbm);
        bool success = ilbm_load(&state.ilbm, (ilbm_range_t){
            .ptr = (void*)response->data.ptr,
            .size = response->data.size
        });
        if (success) {
            state.fb = sfb_make_framebuffer(&(sfb_framebuffer_desc){
                .width = state.ilbm.width,
                .height = state.ilbm.height,
                .format = SFB_FORMAT_PALETTE8,
                .prescale = 2,
            });
            // NOTE: only need to call sfb_update when either the pixel data or the palette actually changes
            sfb_update(state.fb, &(sfb_update_desc){
                .pixels = { .ptr = state.ilbm.pixels.ptr, .size = state.ilbm.pixels.size },
                .palette = SG_RANGE(state.ilbm.colors),
            });
        } else {
            state.load.failed = true;
        }
    } else if (response->failed) {
        state.load.pending = false;
        state.load.failed = true;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .window_title = "ilbm-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
