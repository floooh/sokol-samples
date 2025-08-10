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

static sapp_image_desc generate_image(const int dim, const int color_offset, int highlight_x = -1, int highlight_y = -1)
{
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

    const int num_pixels = dim * dim;
    const size_t sz = (size_t)num_pixels * sizeof(uint32_t);
    sapp_image_desc img_desc;
    img_desc.width = dim;
    img_desc.height = dim;
    img_desc.pixels.ptr = (uint32_t*) calloc(sz, 1);
    img_desc.pixels.size = sz;

    uint32_t* dst = (uint32_t*) img_desc.pixels.ptr;
    const uint32_t* dst_end = dst + num_pixels;
    const uint32_t blank = 0x00FFFFFF;
    const uint32_t shadow = 0xFF000000;
    SOKOL_ASSERT((dim % 8) == 0);
    const int scale = dim / 8;
    for (int ty = 0, y = 0; ty < 8; ty++) {
        const uint32_t color = colors[(ty+(8-color_offset))%8];
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

    // right shadow
    dst = (uint32_t*) img_desc.pixels.ptr;
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

    // bottom shadow
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

    // hotspot highlight
    if (highlight_x != -1 && highlight_y != -1) {
        const uint32_t highlight = 0xFF0000FF;
        for (int x = 0; x < dim; x++) {
            uint32_t prev_color = blank;
            for (int y = 0; y < dim; y++) {
                const int w = 2;
                if (x > highlight_x - w && x <= highlight_x + w && y > highlight_y - w && y <= highlight_y + w) {
                    const int dst_index = y * dim + x;
                    dst[dst_index] = highlight;
                }
            }
        }
    }

    return img_desc;
}

struct state_t {
    sg_pass_action pass_action;
    sapp_mouse_cursor_image cursor_images[4];
    sapp_mouse_cursor_image cursor_image_hotspot_tl;
    sapp_mouse_cursor_image cursor_image_hotspot_tr;
    sapp_mouse_cursor_image cursor_image_hotspot_bl;
    sapp_mouse_cursor_image cursor_image_hotspot_br;
    sapp_mouse_cursor_image cursor_images_anim[8];
};
static state_t state;

static void init(void) {
    sg_desc desc = { };
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    simgui_desc_t simgui_desc = { };
    simgui_desc.disable_set_mouse_cursor = true, // we want to have control over the cursor, that's the point of this sample
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);

    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { 0.0f, 0.5f, 0.7f, 1.0f };

    sapp_image_desc image_16 = generate_image(16, 0);
    sapp_image_desc image_32 = generate_image(32, 0);
    sapp_image_desc image_64 = generate_image(64, 0);
    sapp_image_desc image_128 = generate_image(128, 0);

    state.cursor_images[0] = sapp_make_mouse_cursor_image(&image_16, 8, 8);
    state.cursor_images[1] = sapp_make_mouse_cursor_image(&image_32, 16, 16);
    state.cursor_images[2] = sapp_make_mouse_cursor_image(&image_64, 32, 32);
    state.cursor_images[3] = sapp_make_mouse_cursor_image(&image_128, 64, 64);

    sapp_image_desc image_tl = generate_image(32, 0, 0, 0);
    state.cursor_image_hotspot_tl = sapp_make_mouse_cursor_image(&image_tl, 0, 0);

    sapp_image_desc image_tr = generate_image(32, 0, 30, 0);
    state.cursor_image_hotspot_tr = sapp_make_mouse_cursor_image(&image_tr, 30, 0);

    sapp_image_desc image_bl = generate_image(32, 0, 0, 30);
    state.cursor_image_hotspot_bl = sapp_make_mouse_cursor_image(&image_bl, 0, 30);

    sapp_image_desc image_br = generate_image(32, 0, 30, 30);
    state.cursor_image_hotspot_br = sapp_make_mouse_cursor_image(&image_br, 30, 30);

    free((void*) image_16.pixels.ptr);
    free((void*) image_32.pixels.ptr);
    free((void*) image_64.pixels.ptr);
    free((void*) image_128.pixels.ptr);
    free((void*) image_tl.pixels.ptr);
    free((void*) image_tr.pixels.ptr);
    free((void*) image_bl.pixels.ptr);
    free((void*) image_br.pixels.ptr);

    for (int i = 0; i < 8; i++) {
        sapp_image_desc image_32 = generate_image(32, i);
        state.cursor_images_anim[i] = sapp_make_mouse_cursor_image(&image_32, 16, 16);
        free((void*) image_32.pixels.ptr);
    }
}

static void event(const sapp_event* e) {
    simgui_handle_event(e);
}

static bool draw_cursor_panel(const char* text, float panel_width, float panel_height) {
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
        ImGui::SameLine();
        if (draw_cursor_panel("128x128", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_images[3];
        }

        if (draw_cursor_panel("hotspot top-left", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_image_hotspot_tl;
        }
        ImGui::SameLine();
        if (draw_cursor_panel("hotspot top-right", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_image_hotspot_tr;
        }
        ImGui::SameLine();
        if (draw_cursor_panel("hotspot bottom-left", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_image_hotspot_bl;
        }
        ImGui::SameLine();
        if (draw_cursor_panel("hotspot bottom-right", panel_width, panel_height)) {
            cursor_image_to_set = state.cursor_image_hotspot_br;
        }

        if (draw_cursor_panel("animated", panel_width, panel_height)) {
            static int frame_count = 0;
            frame_count++;
            cursor_image_to_set = state.cursor_images_anim[(frame_count/15)%8];
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
    for (int i = 0; i < 4; i++) {
        sapp_destroy_mouse_cursor_image(state.cursor_images[i]);
    }
    sapp_destroy_mouse_cursor_image(state.cursor_image_hotspot_tl);
    sapp_destroy_mouse_cursor_image(state.cursor_image_hotspot_tr);
    sapp_destroy_mouse_cursor_image(state.cursor_image_hotspot_bl);
    sapp_destroy_mouse_cursor_image(state.cursor_image_hotspot_br);
    for (int i = 0; i < 8; i++) {
        sapp_destroy_mouse_cursor_image(state.cursor_images_anim[i]);
    }
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
