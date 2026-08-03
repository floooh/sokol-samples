//------------------------------------------------------------------------------
//  writeimage-sapp.c
//
//  Test the new sg_write_image_* functions.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_LETTERBOX_IMPL
#include "sokol_letterbox.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#define SOKOL_APP_IMGUI_IMPL
#include "sokol_app_imgui.h"
#include "writeimage-sapp.glsl.h"

#define IMG_NUM_MIPMAPS (8)
#define IMG_WIDTH (1 << IMG_NUM_MIPMAPS)
#define IMG_HEIGHT (1 << IMG_NUM_MIPMAPS)
#define IMG_NUM_SLICES (6)
#define IMG_BYTES_PER_PIXEL (4)

typedef enum { IMGTYPE_2D, IMGTYPE_CUBE, IMGTYPE_3D, IMGTYPE_ARRAY, NUM_IMGTYPES } image_type_t;
static const char* imgtype_str[] = { "2D", "Cube Map", "3D", "2D Array" };

static struct {
    sg_pipeline pips[NUM_IMGTYPES];
    struct {
        int image_type; // image_type_t
        int max_x, max_y, max_slice;
        int max_width, max_height, max_num_slices;
        struct {
            bool dirty;
            struct {
                int offset;
                bool use_defaults;
                int bytes_per_row;
                int bytes_per_slice;
            } src;
            struct {
                int mip_level;
                int x, y, slice;
            } dst;
            struct {
                bool use_defaults;
                int width, height, num_slices;
            } size;
        } write;
        struct {
            int slice;
        } display;
    } ui;
} state = {
    .ui.write.src.use_defaults = true,
    .ui.write.size.use_defaults = true,
};

static void ui(void);
static void ui_update_deps(bool img_type_changed);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgimgui_setup(&(sgimgui_desc_t){0});
    sappimgui_setup();
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    ui_update_deps(true);
    state.ui.write.dirty = false;
}

static void frame(void) {
    ui();

    sg_begin_pass(&(sg_pass){ .swapchain = sglue_swapchain() });
    simgui_render();
    sg_end_pass();
    sg_commit();

}

static void input(const sapp_event* ev) {
    sappimgui_track_event(ev);
    simgui_handle_event(ev);
}

static void cleanup(void) {
    sappimgui_shutdown();
    sgimgui_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

static int ui_max(int v0, int v1) {
    return (v0 > v1) ? v0 : v1;
}

static int ui_min(int v0, int v1) {
    return (v0 < v1) ? v0 : v1;
}

static int ui_mip_dim(int base, int mip_level) {
    return ui_max(base >> mip_level, 1);
}

static void ui_update_deps(bool img_type_changed) {
    state.ui.write.dirty = true;
    if (img_type_changed) {
        state.ui.display.slice = 0;
    }

    // compute current max dst location values
    const int mip_level = state.ui.write.dst.mip_level;
    const int mip_width = ui_mip_dim(IMG_WIDTH, mip_level);
    const int mip_height = ui_mip_dim(IMG_HEIGHT, mip_level);
    const int mip_depth = ui_mip_dim(IMG_NUM_SLICES, mip_level);
    state.ui.max_x = mip_width - 1;
    state.ui.max_y = mip_height - 1;
    switch (state.ui.image_type) {
        case IMGTYPE_2D:
            state.ui.max_slice = 0;
            break;
        case IMGTYPE_3D:
            state.ui.max_slice = mip_depth - 1;
            break;
        default:
            state.ui.max_slice = IMG_NUM_SLICES - 1;
            break;
    }

    // clamp dst location values
    state.ui.write.dst.x = ui_min(state.ui.write.dst.x, state.ui.max_x);
    state.ui.write.dst.y = ui_min(state.ui.write.dst.y, state.ui.max_y);
    state.ui.write.dst.slice = ui_min(state.ui.write.dst.slice, state.ui.max_slice);

    // compute max size values
    state.ui.max_width = mip_width - state.ui.write.dst.x;
    state.ui.max_height = mip_height - state.ui.write.dst.y;
    switch (state.ui.image_type) {
        case IMGTYPE_2D:
            state.ui.max_num_slices = 1;
            break;
        case IMGTYPE_3D:
            state.ui.max_num_slices = mip_depth - state.ui.write.dst.slice;
            break;
        default:
            state.ui.max_num_slices = IMG_NUM_SLICES - state.ui.write.dst.slice;
            break;
    }

    // clamp size values
    if (state.ui.write.size.use_defaults || img_type_changed) {
        state.ui.write.size.width = state.ui.max_width;
        state.ui.write.size.height = state.ui.max_height;
        state.ui.write.size.num_slices = state.ui.max_num_slices;
    } else {
        state.ui.write.size.width = ui_min(state.ui.write.size.width, state.ui.max_width);
        state.ui.write.size.height = ui_min(state.ui.write.size.height, state.ui.max_height);
        state.ui.write.size.num_slices = ui_min(state.ui.write.size.num_slices, state.ui.max_num_slices);
    }
}

static void ui(void) {
    sappimgui_track_frame();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .dpi_scale = sapp_dpi_scale(),
        .delta_time = sapp_frame_duration(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        sappimgui_draw_menu("sokol-app");
        igEndMainMenuBar();
    }
    sappimgui_draw();
    sgimgui_draw();
    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        igSeparatorText("Display Options");
        igSliderInt("Slice##display", &state.ui.display.slice, 0, state.ui.max_slice);
        igSeparatorText("Write Options");
        if (igComboChar("Image Type", &state.ui.image_type, imgtype_str, IM_ARRAYSIZE(imgtype_str))) {
            ui_update_deps(true);
        }
        igText("Write Source:");
        if (igSliderInt("Offset:", &state.ui.write.src.offset, 0, 1024)) {
            // must be multiple of bytes-per-pixel
            state.ui.write.src.offset &= ~(IMG_BYTES_PER_PIXEL-1);
            ui_update_deps(false);
        }
        igText("  FIXME: Bytes Per Row");
        igText("  FIXME: Bytes Per Slice");
        igText("Write Destination:");
        if (igSliderInt("Mip Level", &state.ui.write.dst.mip_level, 0, IMG_NUM_MIPMAPS)) {
            ui_update_deps(false);
        }
        if (igSliderInt("X", &state.ui.write.dst.x, 0, state.ui.max_x)) {
            ui_update_deps(false);
        }
        if (igSliderInt("Y", &state.ui.write.dst.y, 0, state.ui.max_y)) {
            ui_update_deps(false);
        }
        if (igSliderInt("Slice##write", &state.ui.write.dst.slice, 0, state.ui.max_slice)) {
            ui_update_deps(false);
        }
        igText("Write Size:");
        if (igCheckbox("Use Defaults##sizes", &state.ui.write.size.use_defaults)) {
            ui_update_deps(false);
        }
        igBeginDisabled(state.ui.write.size.use_defaults);
        if (igSliderInt("Width", &state.ui.write.size.width, 1, state.ui.max_width)) {
            ui_update_deps(false);
        }
        if (igSliderInt("Height", &state.ui.write.size.height, 1, state.ui.max_height)) {
            ui_update_deps(false);
        }
        if (igSliderInt("Num Slices", &state.ui.write.size.num_slices, 1, state.ui.max_num_slices)) {
            ui_update_deps(false);
        }
        igEndDisabled();
        igBeginDisabled(!state.ui.write.dirty);
        const bool dirty = state.ui.write.dirty;
        if (dirty) {
            igPushStyleColorImVec4(ImGuiCol_Button, (ImVec4){ 1.0f, 0.0f, 0.0f, 1.0f });
        }
        if (igButton("Apply Changes")) {
            state.ui.write.dirty = false;
            // FIXME: recreate image
        }
        if (dirty) {
            igPopStyleColor();
        }
        igEndDisabled();
    }
    igEnd();
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
        .window_title = "writeimage-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
