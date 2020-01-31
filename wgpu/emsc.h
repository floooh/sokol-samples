#pragma once
/* common emscripten platform helper functions */
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

static const char* _emsc_canvas_name = 0;
static double _emsc_width = 0;
static double _emsc_height = 0;

enum {
    EMSC_NONE = 0,
    EMSC_ANTIALIAS = (1<<1)
};

/* track CSS element size changes and update the WebGL canvas size */
static EM_BOOL _emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    emscripten_get_element_css_size(_emsc_canvas_name, &_emsc_width, &_emsc_height);
    emscripten_set_canvas_element_size(_emsc_canvas_name, _emsc_width, _emsc_height);
    return true;
}

/* initialize WebGPU and canvas */
static void emsc_init(const char* canvas_name, int flags) {
    _emsc_canvas_name = canvas_name;
    emscripten_get_element_css_size(canvas_name, &_emsc_width, &_emsc_height);
    emscripten_set_canvas_element_size(canvas_name, _emsc_width, _emsc_height);
    emscripten_set_resize_callback(0, 0, false, _emsc_size_changed);
}

static int emsc_width() {
    return (int) _emsc_width;
}

static int emsc_height() {
    return (int) _emsc_height;
}
