//------------------------------------------------------------------------------
//  cursor-sapp.cc
//  Showcases support for system built-in and custom mouse cursor images
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#include <stdio.h> // snprintf

static const char* mousecursor_to_str(sapp_mouse_cursor t) {
    switch (t) {
        case SAPP_MOUSECURSOR_DEFAULT: return "DEFAULT";
        case SAPP_MOUSECURSOR_ARROW: return "ARROW";
        case SAPP_MOUSECURSOR_IBEAM: return "IBEAM";
        case SAPP_MOUSECURSOR_CROSSHAIR: return "CROSSHAIR";
        case SAPP_MOUSECURSOR_POINTING_HAND: return "POINTING_HAND";
        case SAPP_MOUSECURSOR_RESIZE_EW: return "RESIZE_EW";
        case SAPP_MOUSECURSOR_RESIZE_NS: return "RESIZE_NS";
        case SAPP_MOUSECURSOR_RESIZE_NWSE: return "RESIZE_NWSE";
        case SAPP_MOUSECURSOR_RESIZE_NESW: return "RESIZE_NESW";
        case SAPP_MOUSECURSOR_RESIZE_ALL: return "RESIZE_ALL";
        case SAPP_MOUSECURSOR_NOT_ALLOWED: return "NOT_ALLOWED";
        case SAPP_MOUSECURSOR_CUSTOM_IMAGE: return "CUSTOM_IMAGE";
        default: return "???";
    }
}

static sapp_icon_desc sokol_icon(void) {
    uint32_t* default_icon_pixels = 0;
    sapp_icon_desc default_icon_desc = {};

    const int num_icons = 3;
    const int icon_sizes[3] = { 16, 32, 64 };   // must be multiple of 8!

    // allocate a pixel buffer for all icon pixels
    int all_num_pixels = 0;
    for (int i = 0; i < num_icons; i++) {
        all_num_pixels += icon_sizes[i] * icon_sizes[i];
    }
    default_icon_pixels = (uint32_t*) calloc((size_t)all_num_pixels * sizeof(uint32_t), 1);

    // initialize default_icon_desc struct
    uint32_t* dst = default_icon_pixels;
    const uint32_t* dst_end = dst + all_num_pixels;
    (void)dst_end; // silence unused warning in release mode
    for (int i = 0; i < num_icons; i++) {
        const int dim = (int) icon_sizes[i];
        const int num_pixels = dim * dim;
        sapp_image_desc* img_desc = &default_icon_desc.images[i];
        img_desc->width = dim;
        img_desc->height = dim;
        img_desc->pixels.ptr = dst;
        img_desc->pixels.size = (size_t)num_pixels * sizeof(uint32_t);
        dst += num_pixels;
    }
    SOKOL_ASSERT(dst == dst_end);

    // Amstrad CPC font 'S'
    const uint8_t tile[8] = {
        0x3C,
        0x66,
        0x60,
        0x3C,
        0x06,
        0x66,
        0x3C,
        0x00,
    };
    // rainbow colors
    const uint32_t colors[8] = {
        0xFF4370FF,
        0xFF26A7FF,
        0xFF58EEFF,
        0xFF57E1D4,
        0xFF65CC9C,
        0xFF6ABB66,
        0xFFF5A542,
        0xFFC2577E,
    };
    dst = default_icon_pixels;
    const uint32_t blank = 0x00FFFFFF;
    const uint32_t shadow = 0xFF000000;
    for (int i = 0; i < num_icons; i++) {
        const int dim = icon_sizes[i];
        SOKOL_ASSERT((dim % 8) == 0);
        const int scale = dim / 8;
        for (int ty = 0, y = 0; ty < 8; ty++) {
            const uint32_t color = colors[ty];
            for (int sy = 0; sy < scale; sy++, y++) {
                uint8_t bits = tile[ty];
                for (int tx = 0, x = 0; tx < 8; tx++, bits<<=1) {
                    uint32_t pixel = (0 == (bits & 0x80)) ? blank : color;
                    for (int sx = 0; sx < scale; sx++, x++) {
                        SOKOL_ASSERT(dst < dst_end);
                        *dst++ = pixel;
                    }
                }
            }
        }
    }
    SOKOL_ASSERT(dst == dst_end);

    // right shadow
    dst = default_icon_pixels;
    for (int i = 0; i < num_icons; i++) {
        const int dim = icon_sizes[i];
        for (int y = 0; y < dim; y++) {
            uint32_t prev_color = blank;
            for (int x = 0; x < dim; x++) {
                const int dst_index = y * dim + x;
                const uint32_t cur_color = dst[dst_index];
                if ((cur_color == blank) && (prev_color != blank)) {
                    dst[dst_index] = shadow;
                }
                prev_color = cur_color;
            }
        }
        dst += dim * dim;
    }
    SOKOL_ASSERT(dst == dst_end);

    // bottom shadow
    dst = default_icon_pixels;
    for (int i = 0; i < num_icons; i++) {
        const int dim = icon_sizes[i];
        for (int x = 0; x < dim; x++) {
            uint32_t prev_color = blank;
            for (int y = 0; y < dim; y++) {
                const int dst_index = y * dim + x;
                const uint32_t cur_color = dst[dst_index];
                if ((cur_color == blank) && (prev_color != blank)) {
                    dst[dst_index] = shadow;
                }
                prev_color = cur_color;
            }
        }
        dst += dim * dim;
    }
    SOKOL_ASSERT(dst == dst_end);
    return default_icon_desc;
}

struct state_t {
    sg_pass_action pass_action;
    sapp_mouse_cursor_image cursor_images[3];
};
static state_t state;

static void init(void) {
    sg_desc desc = { };
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    simgui_desc_t simgui_desc = {
        .disable_set_mouse_cursor = true, // we want to have control over the cursor, that's the point of this sample
    };
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);

    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { 0.0f, 0.5f, 0.7f, 1.0f };

    sapp_icon_desc icon = sokol_icon();
    state.cursor_images[0] = sapp_make_mouse_cursor_image(&icon.images[0], 8, 8);
    state.cursor_images[1] = sapp_make_mouse_cursor_image(&icon.images[1], 16, 16);
    state.cursor_images[2] = sapp_make_mouse_cursor_image(&icon.images[2], 32, 32);

}

static void event(const sapp_event* e) {
    simgui_handle_event(e);
}

static bool draw_cursor_panel(const char* text, float panel_width, float panel_height) {
    //ImGui::SameLine();
    ImGui::Button(text, ImVec2(panel_width, panel_height));
    bool hovered = ImGui::IsItemHovered();
    return hovered;
}

static void frame(void) {
    const int w = sapp_width();
    const int h = sapp_height();
    simgui_new_frame({ w, h, sapp_frame_duration(), sapp_dpi_scale() });

    const float panel_width = 160.0f - ImGui::GetStyle().FramePadding.x;
    const float panel_height = 115.0f;
    const float panel_width_with_padding = panel_width + ImGui::GetStyle().FramePadding.x;
    const float pad = 5.0f;
    float pos_x = ImGui::GetStyle().WindowPadding.x;
    ImVec2 padded_size = ImGui::GetIO().DisplaySize;
    padded_size.x -= 2.0f*pad;
    padded_size.y -= 2.0f*pad;
    ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(padded_size, ImGuiCond_Always);

    sapp_mouse_cursor cursor_to_set = SAPP_MOUSECURSOR_DEFAULT;
    sapp_mouse_cursor_image cursor_image_to_set = {0};

    if (ImGui::Begin("Cursors", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("System cursors:");
        for (int i = SAPP_MOUSECURSOR_DEFAULT; i < SAPP_MOUSECURSOR_CUSTOM_IMAGE; i++) {
            sapp_mouse_cursor cur = (sapp_mouse_cursor) i;
            if (i % 5 != 0) {
                ImGui::SameLine();
            }
            if (draw_cursor_panel(mousecursor_to_str(cur), panel_width, panel_height)) {
                cursor_to_set = cur;
            }
        }

        ImGui::Separator();
        ImGui::Text("Custom image cursors:");
        if (draw_cursor_panel("16x16", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_images[0];
        }
        ImGui::SameLine();
        if (draw_cursor_panel("32x32", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_images[1];
        }
        ImGui::SameLine();
        if (draw_cursor_panel("64x64", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_images[2];
        }
    }
    ImGui::End();

    if (cursor_image_to_set.opaque != 0) {
        sapp_set_mouse_cursor_image(cursor_image_to_set);
    } else {
        sapp_set_mouse_cursor(cursor_to_set);
    }

    sg_pass pass = { };
    pass.action = state.pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.event_cb = event;
    desc.cleanup_cb = cleanup;
    desc.width = 832;
    desc.height = 600;
    desc.window_title = "Cursors";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}
