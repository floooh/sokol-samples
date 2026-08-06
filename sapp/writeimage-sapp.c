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
    sg_image img;
    sg_view view;
    sg_sampler smp;
    sg_pipeline pip;
    sg_range mip_data;
    struct {
        int image_type; // image_type_t
        // computes bounds
        int min_bytes_per_row;
        int max_bytes_per_row;
        int min_bytes_per_slice;
        int max_bytes_per_slice;
        int max_x, max_y, max_slice;
        int max_width, max_height, max_num_slices;
        bool dirty;
        struct {
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
            int mip_level;
            int slice;
        } display;
    } ui;
} state = {
    .ui.write.src.use_defaults = true,
    .ui.write.size.use_defaults = true,
};

static void ui(void);
static void ui_update_deps(bool img_type_changed);
static void ui_apply_changes(void);
static void discard_resources(void);
static void create_resources(void);

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
    ui_apply_changes();
}

static void frame(void) {
    ui();
    const sg_pass_action action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = state.ui.dirty
                ? (sg_color){ 0.5f, 0.0f, 0.0f, 1.0f }
                : (sg_color){ 0.0f, 0.5f, 0.0f, 1.0f },
        },
    };
    const fs_params_t fs_params = {
        .miplevel = state.ui.display.mip_level,
        .slice = state.ui.display.slice,
    };
    const slbx_viewport vp = slbx_letterbox(sapp_width(), sapp_height(), &(slbx_letterbox_desc){
        .border = { .top = 30, .bottom = 20, .left = 20, .right = 20 },
        .content_aspect_ratio = 1.0f,
    });

    sg_begin_pass(&(sg_pass){ .action = action, .swapchain = sglue_swapchain() });
    sg_apply_viewport(vp.x, vp.y, vp.width, vp.height, true);
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[0] = state.view,
        .samplers[0] = state.smp,
    });
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(fs_params));
    // draw 'fullscreen triangle'
    sg_draw(0, 3, 1);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    sappimgui_track_event(ev);
    simgui_handle_event(ev);
}

static void cleanup(void) {
    discard_resources();
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
    state.ui.dirty = true;
    if (img_type_changed) {
        state.ui.display.mip_level = 0;
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

    // fix up src options
    state.ui.min_bytes_per_row = mip_width * IMG_BYTES_PER_PIXEL;
    state.ui.max_bytes_per_row = 2048;
    if (state.ui.write.src.use_defaults) {
        state.ui.write.src.bytes_per_row = state.ui.min_bytes_per_row;
    } else {
        const int min_bpr = state.ui.min_bytes_per_row;
        const int max_bpr = state.ui.max_bytes_per_row;
        state.ui.write.src.bytes_per_row = ui_max(ui_min(state.ui.write.src.bytes_per_row, max_bpr), min_bpr);
    }
    // offset and bytes-per-row must be multiple of pixel size
    const int bpp_mask = ~(IMG_BYTES_PER_PIXEL - 1);
    state.ui.write.src.offset &= bpp_mask;
    state.ui.write.src.bytes_per_row &= bpp_mask;

    // NOTE: fix up bytes-per-slice *after* bytes-per-row has been fixed
    const int bpr = state.ui.write.src.bytes_per_row;
    state.ui.min_bytes_per_slice = bpr * mip_height;
    state.ui.max_bytes_per_slice = bpr * 2048;
    if (state.ui.write.src.use_defaults) {
        state.ui.write.src.bytes_per_slice = state.ui.min_bytes_per_slice;
    } else {
        const int min_bps = state.ui.min_bytes_per_slice;
        const int max_bps = state.ui.max_bytes_per_slice;
        state.ui.write.src.bytes_per_slice = ui_max(ui_min(state.ui.write.src.bytes_per_slice, max_bps), min_bps);
    }
    // bytes per slice must be a multiple of bytes per row
    state.ui.write.src.bytes_per_slice = (state.ui.write.src.bytes_per_slice / bpr) * bpr;
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
        if (igComboChar("Image Type", &state.ui.image_type, imgtype_str, IM_ARRAYSIZE(imgtype_str))) {
            ui_update_deps(true);
        }
        igSeparatorText("Display Options");
        igSliderInt("Mip Level##display", &state.ui.display.mip_level, 0, IMG_NUM_MIPMAPS - 1);
        igSliderInt("Slice##display", &state.ui.display.slice, 0, state.ui.max_slice);
        igSeparatorText("Write Options");
        igText("Write Source:");
        if (igSliderInt("Offset:", &state.ui.write.src.offset, 0, 1024)) {
            ui_update_deps(false);
        }
        if (igCheckbox("Use Defaults##pitch", &state.ui.write.src.use_defaults)) {
            ui_update_deps(false);
        }
        igBeginDisabled(state.ui.write.src.use_defaults);
        if (igSliderInt("Bytes Per Row", &state.ui.write.src.bytes_per_row, state.ui.min_bytes_per_row, state.ui.max_bytes_per_row)) {
            ui_update_deps(false);
        }
        if (igSliderInt("Bytes Per Slice", &state.ui.write.src.bytes_per_slice, state.ui.min_bytes_per_slice, state.ui.max_bytes_per_slice)) {
            ui_update_deps(false);
        }
        igEndDisabled();
        igText("Write Destination:");
        if (igSliderInt("Mip Level##dst", &state.ui.write.dst.mip_level, 0, IMG_NUM_MIPMAPS - 1)) {
            ui_update_deps(false);
        }
        if (igSliderInt("X", &state.ui.write.dst.x, 0, state.ui.max_x)) {
            ui_update_deps(false);
        }
        if (igSliderInt("Y", &state.ui.write.dst.y, 0, state.ui.max_y)) {
            ui_update_deps(false);
        }
        if (igSliderInt("Slice##dst", &state.ui.write.dst.slice, 0, state.ui.max_slice)) {
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
        igBeginDisabled(!state.ui.dirty);
        const bool dirty = state.ui.dirty;
        if (dirty) {
            igPushStyleColorImVec4(ImGuiCol_Button, (ImVec4){ 1.0f, 0.0f, 0.0f, 1.0f });
        }
        if (igButton("Apply Changes")) {
            ui_apply_changes();
        }
        if (dirty) {
            igPopStyleColor();
        }
        igEndDisabled();
    }
    igEnd();
}

static void ui_apply_changes(void) {
    assert(state.ui.dirty);
    state.ui.dirty = false;
    state.ui.display.mip_level = state.ui.write.dst.mip_level;
    state.ui.display.slice = state.ui.write.dst.slice;
    discard_resources();
    create_resources();
}

static void alloc_mip_data(void) {
    assert(0 == state.mip_data.ptr);
    assert(0 == state.mip_data.size);
    assert(state.ui.write.src.bytes_per_slice > 0);
    state.mip_data.size = state.ui.write.src.offset + IMG_NUM_SLICES * state.ui.write.src.bytes_per_slice;
    state.mip_data.ptr = calloc(state.mip_data.size, 1);
    assert(state.mip_data.ptr);
}

static void free_mip_data(void) {
    if (state.mip_data.ptr) {
        free((void*)state.mip_data.ptr);
        state.mip_data.ptr = 0;
        state.mip_data.size = 0;
    }
}

static void pixel(int x, int y, int slice, uint32_t rgba) {
    assert(x <= state.ui.max_x);
    assert(y <= state.ui.max_y);
    assert(slice <= state.ui.max_slice);
    const int u32pr = state.ui.write.src.bytes_per_row >> 2;
    const int u32ps = state.ui.write.src.bytes_per_slice >> 2;
    const int u32offset = state.ui.write.src.offset >> 2;
    const int idx = u32offset + slice * u32ps + y * u32pr + x;
    assert((idx << 2) < (IMG_NUM_SLICES * state.ui.write.src.bytes_per_slice));
    assert(state.mip_data.ptr);
    uint32_t* ptr = (uint32_t*)state.mip_data.ptr;
    ptr[idx] = rgba;
}

static void populate_slice(int slice, uint32_t rgba0, uint32_t rgba1) {
    const int w = state.ui.max_x + 1;
    const int h = state.ui.max_y + 1;
    const int ww = w / 2;
    const int hh = h / 2;
    for (int y = 0; y < hh; y++) {
        for (int x = 0; x < ww; x++) {
            uint32_t c;
            if ((y & 1 && x >= y) || (x & 1 && y >= x)) {
                c = rgba0;
            } else {
                c = rgba1;
            }
            const int xx = w - x - 1;
            const int yy = h - y - 1;
            pixel(x, y, slice, c);
            pixel(y, xx, slice, c);
            pixel(yy, x, slice, c);
            pixel(yy, xx, slice, c);
        }
    }
}

static void populate_mipmap_data(void) {
    const int num_slices = state.ui.max_slice + 1;
    static const uint32_t palette[IMG_NUM_SLICES] = {
        0xFFFF0000,
        0xFF00FF00,
        0xFF0000FF,
        0xFFFFFF00,
        0xFF00FFFF,
        0xFFFF00FF,
    };
    for (int slice = 0; slice < num_slices; slice++) {
        populate_slice(slice, 0xFF000000, palette[slice]);
    }
}

static sg_image_type as_sg_image_type(image_type_t t) {
    switch (t) {
        case IMGTYPE_CUBE: return SG_IMAGETYPE_CUBE;
        case IMGTYPE_3D: return SG_IMAGETYPE_3D;
        case IMGTYPE_ARRAY: return SG_IMAGETYPE_ARRAY;
        default: return SG_IMAGETYPE_2D;
    }
}

static const sg_shader_desc* select_shader_by_image_type(image_type_t t) {
    const sg_backend backend = sg_query_backend();
    switch (t) {
        case IMGTYPE_CUBE: SOKOL_ASSERT(false && "FIXME!");
        case IMGTYPE_3D: SOKOL_ASSERT(false && "FIXME");
        case IMGTYPE_ARRAY: return texarray_shader_desc(backend);
        default: return tex2d_shader_desc(backend);
    }
}

static void create_resources(void) {
    // allocate and populate slice buffer
    alloc_mip_data();
    populate_mipmap_data();

    // create image in unsealed state
    const sg_image_type img_type = as_sg_image_type(state.ui.image_type);
    state.img = sg_make_image(&(sg_image_desc){
        .type = img_type,
        .usage.write_unsealed = true,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .num_slices = img_type == SG_IMAGETYPE_2D ? 1 : IMG_NUM_SLICES,
        .num_mipmaps = IMG_NUM_MIPMAPS,
        .label = "test-image",
    });

    // populate one mipmap with data
    const bool src_defaults = state.ui.write.src.use_defaults;
    const bool size_defaults = state.ui.write.size.use_defaults;
    sg_write_image_unsealed(&(sg_write_image_desc){
        .src = {
            .data = state.mip_data,
            .offset = state.ui.write.src.offset,
            .bytes_per_row = src_defaults ? 0 : state.ui.write.src.bytes_per_row,
            .bytes_per_slice = src_defaults ? 0 : state.ui.write.src.bytes_per_slice,
        },
        .dst = {
            .image = state.img,
            .mip_level = state.ui.write.dst.mip_level,
            .x = state.ui.write.dst.x,
            .y = state.ui.write.dst.y,
            .slice = state.ui.write.dst.slice,
        },
        .size = {
            .width = size_defaults ? 0 : state.ui.write.size.width,
            .height = size_defaults ? 0 : state.ui.write.size.height,
            .num_slices = size_defaults ? 0 : state.ui.write.size.num_slices,
        },
    });
    sg_seal_image(state.img);
    free_mip_data();

    // view and sampler
    state.view = sg_make_view(&(sg_view_desc){
        .texture.image = state.img,
        .label = "test-image-view",
    });
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .mipmap_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_w = SG_WRAP_CLAMP_TO_EDGE,
        .label = "test-sampler",
    });

    // pipeline object for a bufferless 'fullscreen triangle'
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(select_shader_by_image_type(state.ui.image_type)),
        .label = "test-pipeline",
    });
}

static void discard_resources(void) {
    sg_destroy_image(state.img);
    sg_destroy_view(state.view);
    sg_destroy_sampler(state.smp);
    sg_destroy_pipeline(state.pip);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 1024,
        .height = 768,
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .window_title = "writeimage-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
