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
        case SAPP_MOUSECURSOR_CUSTOM_0: return "SAPP_MOUSECURSOR_CUSTOM_0";
        case SAPP_MOUSECURSOR_CUSTOM_1: return "SAPP_MOUSECURSOR_CUSTOM_1";
        case SAPP_MOUSECURSOR_CUSTOM_2: return "SAPP_MOUSECURSOR_CUSTOM_2";
        case SAPP_MOUSECURSOR_CUSTOM_3: return "SAPP_MOUSECURSOR_CUSTOM_3";
        case SAPP_MOUSECURSOR_CUSTOM_4: return "SAPP_MOUSECURSOR_CUSTOM_4";
        case SAPP_MOUSECURSOR_CUSTOM_5: return "SAPP_MOUSECURSOR_CUSTOM_5";
        case SAPP_MOUSECURSOR_CUSTOM_6: return "SAPP_MOUSECURSOR_CUSTOM_6";
        case SAPP_MOUSECURSOR_CUSTOM_7: return "SAPP_MOUSECURSOR_CUSTOM_7";
        case SAPP_MOUSECURSOR_CUSTOM_8: return "SAPP_MOUSECURSOR_CUSTOM_8";
        case SAPP_MOUSECURSOR_CUSTOM_9: return "SAPP_MOUSECURSOR_CUSTOM_9";
        case SAPP_MOUSECURSOR_CUSTOM_10: return "SAPP_MOUSECURSOR_CUSTOM_10";
        case SAPP_MOUSECURSOR_CUSTOM_11: return "SAPP_MOUSECURSOR_CUSTOM_11";
        case SAPP_MOUSECURSOR_CUSTOM_12: return "SAPP_MOUSECURSOR_CUSTOM_12";
        case SAPP_MOUSECURSOR_CUSTOM_13: return "SAPP_MOUSECURSOR_CUSTOM_13";
        case SAPP_MOUSECURSOR_CUSTOM_14: return "SAPP_MOUSECURSOR_CUSTOM_14";
        case SAPP_MOUSECURSOR_CUSTOM_15: return "SAPP_MOUSECURSOR_CUSTOM_15";
        case _SAPP_MOUSECURSOR_NUM: return "CUSTOM_IMAGE";
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
    sapp_mouse_cursor cursors[4];
    sapp_mouse_cursor cursor_hotspot_tl;
    sapp_mouse_cursor cursor_hotspot_tr;
    sapp_mouse_cursor cursor_hotspot_bl;
    sapp_mouse_cursor cursor_hotspot_br;
    sapp_mouse_cursor cursors_anim[8];
    bool customized[_SAPP_MOUSECURSOR_NUM]; // to keep track of which 'system' cursor is currently bound to a custom image.
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
    image_16.cursor_hotspot_x = 8;
    image_16.cursor_hotspot_y = 8;

    sapp_image_desc image_32 = generate_image(32, 0);
    image_32.cursor_hotspot_x = 16;
    image_32.cursor_hotspot_y = 16;

    sapp_image_desc image_64 = generate_image(64, 0);
    image_64.cursor_hotspot_x = 32;
    image_64.cursor_hotspot_y = 32;

    sapp_image_desc image_128 = generate_image(128, 0);
    image_128.cursor_hotspot_x = 64;
    image_128.cursor_hotspot_y = 64;

    state.cursors[0] = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_0, &image_16);
    state.cursors[1] = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_1, &image_32);
    state.cursors[2] = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_2, &image_64);
    state.cursors[3] = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_3, &image_128);

    sapp_image_desc image_tl = generate_image(32, 0, 0, 0);
    image_tl.cursor_hotspot_x = 0;
    image_tl.cursor_hotspot_y = 0;
    state.cursor_hotspot_tl = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_4, &image_tl);

    sapp_image_desc image_tr = generate_image(32, 0, 30, 0);
    image_tr.cursor_hotspot_x = 30;
    image_tr.cursor_hotspot_y = 0;
    state.cursor_hotspot_tr = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_5, &image_tr);

    sapp_image_desc image_bl = generate_image(32, 0, 0, 30);
    image_bl.cursor_hotspot_x = 0;
    image_bl.cursor_hotspot_y = 30;
    state.cursor_hotspot_bl = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_6, &image_bl);

    sapp_image_desc image_br = generate_image(32, 0, 30, 30);
    image_br.cursor_hotspot_x = 30;
    image_br.cursor_hotspot_y = 30;
    state.cursor_hotspot_br = sapp_bind_mouse_cursor_image(SAPP_MOUSECURSOR_CUSTOM_7, &image_br);

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
        image_32.cursor_hotspot_x = 15;
        image_32.cursor_hotspot_y = 15;
        state.cursors_anim[i] = sapp_bind_mouse_cursor_image((sapp_mouse_cursor) (SAPP_MOUSECURSOR_CUSTOM_8 + i), &image_32);
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
    const float pad = 5.0f;
    ImVec2 padded_size = ImGui::GetIO().DisplaySize;
    padded_size.x -= 2.0f*pad;
    padded_size.y -= 2.0f*pad;
    ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(padded_size, ImGuiCond_Always);

    sapp_mouse_cursor cursor_to_set = SAPP_MOUSECURSOR_DEFAULT;

    if (ImGui::Begin("Cursors", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("System cursors:");
        for (int i = SAPP_MOUSECURSOR_DEFAULT; i < SAPP_MOUSECURSOR_CUSTOM_0; i++) {
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
            cursor_to_set = state.cursors[0];
        }
        ImGui::SameLine();
        if (draw_cursor_panel("32x32", panel_width, panel_height)) {
            cursor_to_set = state.cursors[1];
        }
        ImGui::SameLine();
        if (draw_cursor_panel("64x64", panel_width, panel_height)) {
            cursor_to_set = state.cursors[2];
        }
        ImGui::SameLine();
        if (draw_cursor_panel("128x128", panel_width, panel_height)) {
            cursor_to_set = state.cursors[3];
        }

        if (draw_cursor_panel("hotspot top-left", panel_width, panel_height)) {
            cursor_to_set = state.cursor_hotspot_tl;
        }
        ImGui::SameLine();
        if (draw_cursor_panel("hotspot top-right", panel_width, panel_height)) {
            cursor_to_set = state.cursor_hotspot_tr;
        }
        ImGui::SameLine();
        if (draw_cursor_panel("hotspot bottom-left", panel_width, panel_height)) {
            cursor_to_set = state.cursor_hotspot_bl;
        }
        ImGui::SameLine();
        if (draw_cursor_panel("hotspot bottom-right", panel_width, panel_height)) {
            cursor_to_set = state.cursor_hotspot_br;
        }

        if (draw_cursor_panel("animated", panel_width, panel_height)) {
            cursor_to_set = state.cursors_anim[(sapp_frame_count()/15)%8];
        }

        ImGui::Separator();
        ImGui::Text("Overriding \"system\" cursors with custom images:");
        {
            sapp_mouse_cursor cursor = SAPP_MOUSECURSOR_DEFAULT;
            bool* customized = &state.customized[cursor];
            if (ImGui::Button(*customized ? "Restore SAPP_MOUSECURSOR_DEFAULT" : "Customize SAPP_MOUSECURSOR_DEFAULT")) {
                if (!*customized) {
                    sapp_image_desc image_32 = generate_image(32, 0);
                    image_32.cursor_hotspot_x = 16;
                    image_32.cursor_hotspot_y = 16;
                    sapp_bind_mouse_cursor_image(cursor, &image_32);
                    free((void*) image_32.pixels.ptr);
                } else {
                    sapp_unbind_mouse_cursor_image(cursor);
                }
                *customized = !*customized;
            }
        }
        {
            sapp_mouse_cursor cursor = SAPP_MOUSECURSOR_IBEAM;
            bool* customized = &state.customized[cursor];
            if (ImGui::Button(*customized ? "Restore SAPP_MOUSECURSOR_IBEAM" : "Customize SAPP_MOUSECURSOR_IBEAM")) {
                if (!*customized) {
                    sapp_image_desc image_32 = generate_image(32, 0);
                    image_32.cursor_hotspot_x = 16;
                    image_32.cursor_hotspot_y = 16;
                    sapp_bind_mouse_cursor_image(cursor, &image_32);
                    free((void*) image_32.pixels.ptr);
                } else {
                    sapp_unbind_mouse_cursor_image(cursor);
                }
                *customized = !*customized;
            }
            if (*customized) {
                ImGui::SameLine(); ImGui::Text("To see the effect, hover the SAPP_MOUSECURSOR_IBEAM rectangle higher up!");
            }
        }
    }
    ImGui::End();

    sapp_set_mouse_cursor(cursor_to_set);

    sg_pass pass = { };
    pass.action = state.pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    // NOTE: No need to unbind mouse cursors on shutdown, it is done automatically by sokol_app.
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
