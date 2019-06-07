//------------------------------------------------------------------------------
//  events-sapp.cc
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#include <stdio.h>  // snprintf

// convert an sapp_event_type to a human readable string
static const char* eventtype_as_str(sapp_event_type t) {
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
        case SAPP_EVENTTYPE_SUSPENDED: return "SUSPENDED";
        case SAPP_EVENTTYPE_RESUMED: return "RESUMED";
        case SAPP_EVENTTYPE_UPDATE_CURSOR: return "UPDATE_CURSOR";
        default: return "???";
    }
}

// A simple ringbuffer to store recorded events.
// Note that the max number of stored events is
// (NUM_ITEMS - 1), one slot is reserved for
// detecting an empty vs full ringbuffer.
struct item_t {
    int id = 0;
    sapp_event event;

    item_t() { };
    item_t(int _id, const sapp_event& _event): id(_id), event(_event) { };
    void print(char* buf, int buf_size) const {
        snprintf(buf, buf_size, "%d: %s", id, eventtype_as_str(event.type));
        buf[buf_size-1] = 0;
    }
};

struct ringbuffer_t {
    static const int NUM_ITEMS = 64;
    item_t items[NUM_ITEMS];
    int tail = 0;
    int head = 0;

    static int index(int i) {
        return i % NUM_ITEMS;
    }
    bool is_full() const {
        return index(head + 1) == tail;
    }
    bool is_empty() const {
        return head == tail;
    }
    int count() const {
        int c;
        if (head >= tail) {
            c = head - tail;
        }
        else {
            c = (head + NUM_ITEMS) - tail;
        }
        assert((c >= 0) && (c < NUM_ITEMS));
        return c;
    }
    const item_t& item_at(int i) const {
        return items[index(tail + i)];
    }
    void add_item(const item_t& item) {
        assert((head >= 0) && (head < NUM_ITEMS));
        if (is_full()) {
            tail = index(tail + 1);
        }
        items[head] = item;
        head = index(head + 1);
    }
};

// global application state
struct state_t {
    int event_counter = 0;
    int selected_item = -1;
    ringbuffer_t ringbuffer;
    sapp_event cur_mouse_move;
    sapp_event cur_mouse_scroll;
    sapp_event cur_touches_moved;
    bool event_filter[_SAPP_EVENTTYPE_NUM];
    sg_pass_action pass_action;
};
static state_t state;

// sokol-app initialization callback
void init(void) {
    sg_desc desc = { };
    desc.mtl_device = sapp_metal_get_device();
    desc.mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor;
    desc.mtl_drawable_cb = sapp_metal_get_drawable;
    desc.d3d11_device = sapp_d3d11_get_device();
    desc.d3d11_device_context = sapp_d3d11_get_device_context();
    desc.d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view;
    desc.d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view;
    sg_setup(&desc);

    simgui_desc_t simgui_desc = { };
    simgui_setup(&simgui_desc);

    for (int i = 0; i < _SAPP_EVENTTYPE_NUM; i++) {
        state.event_filter[i] = true;
    }
    state.event_filter[SAPP_EVENTTYPE_MOUSE_MOVE] = false;
    state.event_filter[SAPP_EVENTTYPE_MOUSE_SCROLL] = false;
    state.event_filter[SAPP_EVENTTYPE_TOUCHES_MOVED] = false;

    state.pass_action.colors[0].action = SG_ACTION_CLEAR;
    state.pass_action.colors[0].val[0] = 0.0f;
    state.pass_action.colors[0].val[1] = 0.5f;
    state.pass_action.colors[0].val[2] = 0.7f;
    state.pass_action.colors[0].val[3] = 1.0f;
}

// the sokol-sapp event callback stores the events in the ring buffer
// (and forwards the events to imgui)
void event(const sapp_event* e) {
    assert((e->type >= 0) && (e->type < _SAPP_EVENTTYPE_NUM));
    if (state.event_filter[e->type]) {
        item_t item(state.event_counter++, *e);
        state.ringbuffer.add_item(item);
        state.selected_item = item.id;
        
    }
    if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        state.cur_mouse_move = *e;
    }
    else if (e->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        state.cur_mouse_scroll = *e;
    }
    else if (e->type == SAPP_EVENTTYPE_TOUCHES_MOVED) {
        state.cur_touches_moved = *e;
    }
    simgui_handle_event(e);
}

void frame(void) {
    const int w = sapp_width();
    const int h = sapp_height();
    simgui_new_frame(w, h, 1.0/60.0);

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiSetCond_Once);
    if (ImGui::Begin("Event Tracker")) {
        const ringbuffer_t& rb = state.ringbuffer;
        // event list on the left, event-info panel on the right
        char str_buf[128];
        ImGui::BeginChild("event_list", ImVec2(128, 0), true);
        const int num_items = rb.count();
        for (int i = 0; i < num_items; i++) {
            const item_t& item = rb.item_at(i);
            item.print(str_buf, sizeof(str_buf));
            ImGui::PushID(item.id);
            if (ImGui::Selectable(str_buf, state.selected_item == item.id)) {
                state.selected_item = item.id;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("event_panel", ImVec2(0, 0), false);
            ImGui::Text("mouse move:   %4.3f %4.3f\n",
                state.cur_mouse_move.mouse_x,
                state.cur_mouse_move.mouse_y);
            ImGui::Text("mouse scroll: %4.3f %4.3f\n",
                state.cur_mouse_scroll.scroll_x,
                state.cur_mouse_scroll.scroll_y);
            for (int ti = 0; ti < state.cur_touches_moved.num_touches; ti++) {
                ImGui::Text("touch pos %d: %4.3f %4.3f\n", ti,
                    state.cur_touches_moved.touches[ti].pos_x,
                    state.cur_touches_moved.touches[ti].pos_y);
            }
            ImGui::Separator();
        ImGui::EndChild();
    }
    ImGui::End();

    sg_begin_default_pass(&state.pass_action, w, h);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.event_cb = event;
    desc.cleanup_cb = cleanup;
    desc.width = 800;
    desc.height = 600;
    desc.window_title = "Events (sokol app)";
    desc.user_cursor = true;
    desc.gl_force_gles2 = true;
    return desc;
}
