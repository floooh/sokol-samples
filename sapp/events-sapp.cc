//------------------------------------------------------------------------------
//  events-sapp.cc
//  Inspect sokol_app.h events via Dear ImGui.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#include <stdio.h> // snprintf

static const int max_dropped_files = 4;

static const char* eventtype_to_str(sapp_event_type t) {
    switch (t) {
        case SAPP_EVENTTYPE_INVALID: return "INVALID";
        case SAPP_EVENTTYPE_KEY_DOWN: return "KEY_DOWN";
        case SAPP_EVENTTYPE_KEY_UP: return "KEY_UP";
        case SAPP_EVENTTYPE_CHAR: return "CHAR";
        case SAPP_EVENTTYPE_MOUSE_DOWN: return "MOUSE_DOWN";
        case SAPP_EVENTTYPE_MOUSE_UP: return "MOUSE_UP";
        case SAPP_EVENTTYPE_MOUSE_SCROLL: return "MOUSE_SCROLL";
        case SAPP_EVENTTYPE_MOUSE_MOVE: return "MOUSE_MOVE";
        case SAPP_EVENTTYPE_MOUSE_ENTER: return "MOUSE_ENTER";
        case SAPP_EVENTTYPE_MOUSE_LEAVE: return "MOUSE_LEAVE";
        case SAPP_EVENTTYPE_TOUCHES_BEGAN: return "TOUCHES_BEGAN";
        case SAPP_EVENTTYPE_TOUCHES_MOVED: return "TOUCHES_MOVED";
        case SAPP_EVENTTYPE_TOUCHES_ENDED: return "TOUCHES_ENDED";
        case SAPP_EVENTTYPE_TOUCHES_CANCELLED: return "TOUCHES_CANCELLED";
        case SAPP_EVENTTYPE_RESIZED: return "RESIZED";
        case SAPP_EVENTTYPE_ICONIFIED: return "ICONIFIED";
        case SAPP_EVENTTYPE_RESTORED: return "RESTORED";
        case SAPP_EVENTTYPE_FOCUSED: return "FOCUSED";
        case SAPP_EVENTTYPE_UNFOCUSED: return "UNFOCUSED";
        case SAPP_EVENTTYPE_SUSPENDED: return "SUSPENDED";
        case SAPP_EVENTTYPE_RESUMED: return "RESUMED";
        case SAPP_EVENTTYPE_UPDATE_CURSOR: return "UPDATE_CURSOR";
        case SAPP_EVENTTYPE_QUIT_REQUESTED: return "QUIT_REQUESTED";
        case SAPP_EVENTTYPE_CLIPBOARD_PASTED: return "CLIPBOARD_PASTED";
        case SAPP_EVENTTYPE_FILES_DROPPED: return "FILES_DROPPED";
        default: return "???";
    }
}

static const char* keycode_to_str(sapp_keycode k) {
    switch (k) {
        case SAPP_KEYCODE_INVALID:      return "INVALID";
        case SAPP_KEYCODE_SPACE:        return "SPACE";
        case SAPP_KEYCODE_APOSTROPHE:   return "APOSTROPHE";
        case SAPP_KEYCODE_COMMA:        return "COMMA";
        case SAPP_KEYCODE_MINUS:        return "MINUS";
        case SAPP_KEYCODE_PERIOD:       return "PERIOD";
        case SAPP_KEYCODE_SLASH:        return "SLASH";
        case SAPP_KEYCODE_0:            return "0";
        case SAPP_KEYCODE_1:            return "1";
        case SAPP_KEYCODE_2:            return "2";
        case SAPP_KEYCODE_3:            return "3";
        case SAPP_KEYCODE_4:            return "4";
        case SAPP_KEYCODE_5:            return "5";
        case SAPP_KEYCODE_6:            return "6";
        case SAPP_KEYCODE_7:            return "7";
        case SAPP_KEYCODE_8:            return "8";
        case SAPP_KEYCODE_9:            return "9";
        case SAPP_KEYCODE_SEMICOLON:    return "SEMICOLON";
        case SAPP_KEYCODE_EQUAL:        return "EQUAL";
        case SAPP_KEYCODE_A:            return "A";
        case SAPP_KEYCODE_B:            return "B";
        case SAPP_KEYCODE_C:            return "C";
        case SAPP_KEYCODE_D:            return "D";
        case SAPP_KEYCODE_E:            return "E";
        case SAPP_KEYCODE_F:            return "F";
        case SAPP_KEYCODE_G:            return "G";
        case SAPP_KEYCODE_H:            return "H";
        case SAPP_KEYCODE_I:            return "I";
        case SAPP_KEYCODE_J:            return "J";
        case SAPP_KEYCODE_K:            return "K";
        case SAPP_KEYCODE_L:            return "L";
        case SAPP_KEYCODE_M:            return "M";
        case SAPP_KEYCODE_N:            return "N";
        case SAPP_KEYCODE_O:            return "O";
        case SAPP_KEYCODE_P:            return "P";
        case SAPP_KEYCODE_Q:            return "Q";
        case SAPP_KEYCODE_R:            return "R";
        case SAPP_KEYCODE_S:            return "S";
        case SAPP_KEYCODE_T:            return "T";
        case SAPP_KEYCODE_U:            return "U";
        case SAPP_KEYCODE_V:            return "V";
        case SAPP_KEYCODE_W:            return "W";
        case SAPP_KEYCODE_X:            return "X";
        case SAPP_KEYCODE_Y:            return "Y";
        case SAPP_KEYCODE_Z:            return "Z";
        case SAPP_KEYCODE_LEFT_BRACKET: return "LEFT_BRACKET";
        case SAPP_KEYCODE_BACKSLASH:    return "BACKSLASH";
        case SAPP_KEYCODE_RIGHT_BRACKET:    return "RIGHT_BRACKET";
        case SAPP_KEYCODE_GRAVE_ACCENT: return "ACCENT";
        case SAPP_KEYCODE_WORLD_1:      return "WORLD_1";
        case SAPP_KEYCODE_WORLD_2:      return "WORLD_2";
        case SAPP_KEYCODE_ESCAPE:       return "ESCAPE";
        case SAPP_KEYCODE_ENTER:        return "ENTER";
        case SAPP_KEYCODE_TAB:          return "TAB";
        case SAPP_KEYCODE_BACKSPACE:    return "BACKSPACE";
        case SAPP_KEYCODE_INSERT:       return "INSERT";
        case SAPP_KEYCODE_DELETE:       return "DELETE";
        case SAPP_KEYCODE_RIGHT:        return "RIGHT";
        case SAPP_KEYCODE_LEFT:         return "LEFT";
        case SAPP_KEYCODE_DOWN:         return "DOWN";
        case SAPP_KEYCODE_UP:           return "UP";
        case SAPP_KEYCODE_PAGE_UP:      return "PAGE_UP";
        case SAPP_KEYCODE_PAGE_DOWN:    return "PAGE_DOWN";
        case SAPP_KEYCODE_HOME:         return "HOME";
        case SAPP_KEYCODE_END:          return "END";
        case SAPP_KEYCODE_CAPS_LOCK:    return "CAPS_LOCK";
        case SAPP_KEYCODE_SCROLL_LOCK:  return "SCROLL_LOCK";
        case SAPP_KEYCODE_NUM_LOCK:     return "NUM_LOCK";
        case SAPP_KEYCODE_PRINT_SCREEN: return "PRINT_SCREEN";
        case SAPP_KEYCODE_PAUSE:        return "PAUSE";
        case SAPP_KEYCODE_F1:           return "F1";
        case SAPP_KEYCODE_F2:           return "F2";
        case SAPP_KEYCODE_F3:           return "F3";
        case SAPP_KEYCODE_F4:           return "F4";
        case SAPP_KEYCODE_F5:           return "F5";
        case SAPP_KEYCODE_F6:           return "F6";
        case SAPP_KEYCODE_F7:           return "F7";
        case SAPP_KEYCODE_F8:           return "F8";
        case SAPP_KEYCODE_F9:           return "F9";
        case SAPP_KEYCODE_F10:          return "F10";
        case SAPP_KEYCODE_F11:          return "F11";
        case SAPP_KEYCODE_F12:          return "F12";
        case SAPP_KEYCODE_F13:          return "F13";
        case SAPP_KEYCODE_F14:          return "F14";
        case SAPP_KEYCODE_F15:          return "F15";
        case SAPP_KEYCODE_F16:          return "F16";
        case SAPP_KEYCODE_F17:          return "F17";
        case SAPP_KEYCODE_F18:          return "F18";
        case SAPP_KEYCODE_F19:          return "F19";
        case SAPP_KEYCODE_F20:          return "F20";
        case SAPP_KEYCODE_F21:          return "F21";
        case SAPP_KEYCODE_F22:          return "F22";
        case SAPP_KEYCODE_F23:          return "F23";
        case SAPP_KEYCODE_F24:          return "F24";
        case SAPP_KEYCODE_F25:          return "F25";
        case SAPP_KEYCODE_KP_0:         return "KP_0";
        case SAPP_KEYCODE_KP_1:         return "KP_1";
        case SAPP_KEYCODE_KP_2:         return "KP_2";
        case SAPP_KEYCODE_KP_3:         return "KP_3";
        case SAPP_KEYCODE_KP_4:         return "KP_4";
        case SAPP_KEYCODE_KP_5:         return "KP_5";
        case SAPP_KEYCODE_KP_6:         return "KP_6";
        case SAPP_KEYCODE_KP_7:         return "KP_7";
        case SAPP_KEYCODE_KP_8:         return "KP_8";
        case SAPP_KEYCODE_KP_9:         return "KP_9";
        case SAPP_KEYCODE_KP_DECIMAL:   return "KP_DECIMAL";
        case SAPP_KEYCODE_KP_DIVIDE:    return "KP_DIVIDE";
        case SAPP_KEYCODE_KP_MULTIPLY:  return "KP_MULTIPLY";
        case SAPP_KEYCODE_KP_SUBTRACT:  return "KP_SUBTRACT";
        case SAPP_KEYCODE_KP_ADD:       return "KP_ADD";
        case SAPP_KEYCODE_KP_ENTER:     return "KP_ENTER";
        case SAPP_KEYCODE_KP_EQUAL:     return "KP_EQUAL";
        case SAPP_KEYCODE_LEFT_SHIFT:   return "LEFT_SHIFT";
        case SAPP_KEYCODE_LEFT_CONTROL: return "LEFT_CONTROL";
        case SAPP_KEYCODE_LEFT_ALT:     return "LEFT_ALT";
        case SAPP_KEYCODE_LEFT_SUPER:   return "LEFT_SUPER";
        case SAPP_KEYCODE_RIGHT_SHIFT:  return "RIGHT_SHIFT";
        case SAPP_KEYCODE_RIGHT_CONTROL:    return "RIGHT_CONTROL";
        case SAPP_KEYCODE_RIGHT_ALT:    return "RIGHT_ALT";
        case SAPP_KEYCODE_RIGHT_SUPER:  return "RIGHT_SUPER";
        case SAPP_KEYCODE_MENU:         return "MENU";
        default:                        return "???";
    }
}

static const char* button_to_str(sapp_mousebutton btn) {
    switch (btn) {
        case SAPP_MOUSEBUTTON_INVALID:  return "INVALID";
        case SAPP_MOUSEBUTTON_LEFT:     return "LEFT";
        case SAPP_MOUSEBUTTON_RIGHT:    return "RIGHT";
        case SAPP_MOUSEBUTTON_MIDDLE:   return "MIDDLE";
        default:                        return "???";
    }
}

struct item_t {
    sapp_event event = { };
};

struct state_t {
    item_t items[_SAPP_EVENTTYPE_NUM];
    sg_pass_action pass_action;
};
static state_t state;

static void init(void) {
    sg_desc desc = { };
    desc.context = sapp_sgcontext();
    sg_setup(&desc);

    simgui_desc_t simgui_desc = { };
    simgui_setup(&simgui_desc);

    state.pass_action.colors[0].action = SG_ACTION_CLEAR;
    state.pass_action.colors[0].value = { 0.0f, 0.5f, 0.7f, 1.0f };
}

static void event(const sapp_event* e) {
    assert((e->type >= 0) && (e->type < _SAPP_EVENTTYPE_NUM));
    state.items[e->type].event = *e;
    simgui_handle_event(e);

    /* special case: show/hide mouse cursor when pressing SPACE */
    if ((e->type == SAPP_EVENTTYPE_KEY_DOWN) && !e->key_repeat) {
        switch (e->key_code) {
            case SAPP_KEYCODE_SPACE:
                sapp_show_mouse(false);
                break;
            case SAPP_KEYCODE_M:
                sapp_lock_mouse(true);
                break;
            default:
                break;
        }
    }
    else if (e->type == SAPP_EVENTTYPE_KEY_UP) {
        switch (e->key_code) {
            case SAPP_KEYCODE_SPACE:
                sapp_show_mouse(true);
                break;
            case SAPP_KEYCODE_M:
                sapp_lock_mouse(false);
                break;
            default:
                break;
        }
    }
}

static bool is_key_event(sapp_event_type t) {
    switch (t) {
        case SAPP_EVENTTYPE_KEY_DOWN:
        case SAPP_EVENTTYPE_KEY_UP:
        case SAPP_EVENTTYPE_CHAR:
            return true;
        default:
            return false;
    }
}

static bool is_mouse_or_drop_event(sapp_event_type t) {
    switch (t) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
        case SAPP_EVENTTYPE_MOUSE_UP:
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
        case SAPP_EVENTTYPE_MOUSE_MOVE:
        case SAPP_EVENTTYPE_MOUSE_ENTER:
        case SAPP_EVENTTYPE_MOUSE_LEAVE:
        case SAPP_EVENTTYPE_FILES_DROPPED:
            return true;
        default:
            return false;
    }
}

static bool is_touch_event(sapp_event_type t) {
    switch (t) {
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
        case SAPP_EVENTTYPE_TOUCHES_MOVED:
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
        case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
            return true;
        default:
            return false;
    }
}

static void draw_event_info_panel(sapp_event_type type, float width, float height) {
    const sapp_event& ev = state.items[type].event;
    float frame_age = (float)(sapp_frame_count() - ev.frame_count);
    float flash_intensity = (20.0f - frame_age) / 20.0f;
    if (flash_intensity < 0.25f) { flash_intensity = 0.25f; }
    else if (flash_intensity > 1.0f) { flash_intensity = 1.0f; }
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(flash_intensity, 0.25f, 0.25f, 1.0f));
    ImGui::PushID((int)type);
    ImGui::BeginChild("event_panel", ImVec2(width, height), true);
    ImGui::Text("type:         %s", eventtype_to_str(type));
    ImGui::Text("frame:        %d", (uint32_t)ev.frame_count);
    ImGui::Text("modifiers:   ");
    if (0 == ev.modifiers) {
        ImGui::SameLine(); ImGui::Text("NONE");
    }
    else {
        if (0 != (ev.modifiers & SAPP_MODIFIER_SHIFT)) {
            ImGui::SameLine(); ImGui::Text("SHIFT");
        }
        if (0 != (ev.modifiers & SAPP_MODIFIER_CTRL)) {
            ImGui::SameLine(); ImGui::Text("CTRL");
        }
        if (0 != (ev.modifiers & SAPP_MODIFIER_ALT)) {
            ImGui::SameLine(); ImGui::Text("ALT");
        }
        if (0 != (ev.modifiers & SAPP_MODIFIER_SUPER)) {
            ImGui::SameLine(); ImGui::Text("SUPER");
        }
        if (0 != (ev.modifiers & SAPP_MODIFIER_LMB)) {
            ImGui::SameLine(); ImGui::Text("LMB");
        }
        if (0 != (ev.modifiers & SAPP_MODIFIER_RMB)) {
            ImGui::SameLine(); ImGui::Text("RMB");
        }
        if (0 != (ev.modifiers & SAPP_MODIFIER_MMB)) {
            ImGui::SameLine(); ImGui::Text("MMB");
        }
    }
    if (is_key_event(type)) {
        ImGui::Text("key code:     %s", keycode_to_str(ev.key_code));
        ImGui::Text("char code:    0x%05X", ev.char_code);
        ImGui::Text("key repeat:   %s", ev.key_repeat ? "true":"false");
    }
    if (is_mouse_or_drop_event(type)) {
        ImGui::Text("mouse btn:    %s", button_to_str(ev.mouse_button));
        ImGui::Text("mouse pos:    %4.2f %4.2f", ev.mouse_x, ev.mouse_y);
        ImGui::Text("mouse delta:  %4.2f %4.2f", ev.mouse_dx, ev.mouse_dy);
        ImGui::Text("scrolling:    %4.2f %4.2f", ev.scroll_x, ev.scroll_y);
    }
    if (is_touch_event(type)) {
        ImGui::Text("num touches:  %d", ev.num_touches);
        for (int i = 0; i < ev.num_touches; i++) {
            ImGui::Text(" %d id:      0x%X", i, (uint32_t) ev.touches[i].identifier);
            ImGui::Text(" %d pos:     %4.2f %4.2f", i, ev.touches[i].pos_x, ev.touches[i].pos_y);
            ImGui::Text(" %d changed: %s", i, ev.touches[i].changed ? "true":"false");
        }
    }
    if (type == SAPP_EVENTTYPE_CLIPBOARD_PASTED) {
        ImGui::Text("clipboard:    %s", sapp_get_clipboard_string());
    }
    ImGui::Text("window size:  %d %d", ev.window_width, ev.window_height);
    ImGui::Text("fb size:      %d %d", ev.framebuffer_width, ev.framebuffer_height);
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::PopStyleColor();
}

static void frame(void) {

    // test sapp_set_window_title()
    char window_title[64];
    snprintf(window_title, sizeof(window_title), "Events (frame_count=%d, dur=%.3fms)", (int) sapp_frame_count(), sapp_frame_duration()*1000.0);
    sapp_set_window_title(window_title);

    const int w = sapp_width();
    const int h = sapp_height();
    simgui_new_frame({ w, h, sapp_frame_duration(), sapp_dpi_scale() });

    const float panel_width = 240.0f - ImGui::GetStyle().FramePadding.x;
    const float panel_height = 170.0f;
    const float panel_width_with_padding = panel_width + ImGui::GetStyle().FramePadding.x;
    const float pad = 5.0f;
    float pos_x = ImGui::GetStyle().WindowPadding.x;
    ImVec2 padded_size = ImGui::GetIO().DisplaySize;
    padded_size.x -= 2.0f*pad;
    padded_size.y -= 2.0f*pad;
    ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(padded_size, ImGuiCond_Always);
    if (ImGui::Begin("Event Inspector", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Press SPACE key to show/hide the mouse cursor (current status: %s)!", sapp_mouse_shown() ? "SHOWN":"HIDDEN");
        ImGui::Text("Press M key to lock/unlock the mouse cursor (current status: %s)!", sapp_mouse_locked() ? "LOCKED":"UNLOCKED");
        ImGui::Text("dropped files (%d/%d):", sapp_get_num_dropped_files(), max_dropped_files);
        for (int i = 0; i < sapp_get_num_dropped_files(); i++) {
            #if defined(__EMSCRIPTEN__)
            ImGui::Text("    %d: %s (bytes: %d)\n", i, sapp_get_dropped_file_path(i), sapp_html5_get_dropped_file_size(i));
            #else
            ImGui::Text("    %d: %s\n", i, sapp_get_dropped_file_path(i));
            #endif
        }
        for (int i = SAPP_EVENTTYPE_INVALID+1; i < _SAPP_EVENTTYPE_NUM; i++) {
            draw_event_info_panel((sapp_event_type)i, panel_width, panel_height);
            pos_x += panel_width_with_padding;
            if ((pos_x + panel_width_with_padding) < ImGui::GetContentRegionAvail().x) {
                ImGui::SameLine();
            }
            else {
                pos_x = ImGui::GetStyle().WindowPadding.x;
            }
        }
    }
    ImGui::End();

    sg_begin_default_pass(&state.pass_action, w, h);
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
    desc.window_title = "Events";
    desc.user_cursor = true;
    desc.gl_force_gles2 = true;
    desc.enable_clipboard = true;
    desc.enable_dragndrop = true;
    desc.max_dropped_files = max_dropped_files;
    desc.icon.sokol_default = true;
    return desc;
}
