//------------------------------------------------------------------------------
//  wgpu_entry_emsc.c
//  Emscripten-specific WebGPU scaffolding. NOTE: does not scale the swapchain
//  surfaces when the window size changes.
//------------------------------------------------------------------------------
#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include "wgpu_entry.h"

static void emsc_update_canvas_size(wgpu_state_t* state) {
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    emscripten_set_canvas_element_size("#canvas", w, h);
    state->width = (int) w;
    state->height = (int) h;
    printf("canvas size updated: %d %d\n", state->width, state->height);
}

static EM_BOOL emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* userdata) {
    (void)event_type; (void)ui_event;
    wgpu_state_t* state = userdata;
    emsc_update_canvas_size(state);
    wgpu_swapchain_resized(state);
    return true;
}

static struct {
    const char* str;
    wgpu_keycode_t code;
} emsc_keymap[] = {
    { "Backspace",      WGPU_KEY_BACKSPACE },
    { "Tab",            WGPU_KEY_TAB },
    { "Enter",          WGPU_KEY_ENTER },
    { "Escape",         WGPU_KEY_ESCAPE },
    { "End",            WGPU_KEY_END },
    { "Home",           WGPU_KEY_HOME },
    { "ArrowLeft",      WGPU_KEY_LEFT },
    { "ArrowUp",        WGPU_KEY_UP },
    { "ArrowRight",     WGPU_KEY_RIGHT },
    { "ArrowDown",      WGPU_KEY_DOWN },
    { "Delete",         WGPU_KEY_DELETE },
    { 0, WGPU_KEY_INVALID },
};

static wgpu_keycode_t emsc_translate_key(const char* str) {
    int i = 0;
    const char* keystr;
    while (( keystr = emsc_keymap[i].str )) {
        if (0 == strcmp(str, keystr)) {
            return emsc_keymap[i].code;
        }
        i += 1;
    }
    return WGPU_KEY_INVALID;
}

static EM_BOOL emsc_keydown_cb(int type, const EmscriptenKeyboardEvent* ev, void* userdata) {
    (void)type;
    wgpu_state_t* state = (wgpu_state_t*)userdata;
    wgpu_keycode_t wgpu_key = emsc_translate_key(ev->code);
    if ((WGPU_KEY_INVALID != wgpu_key) && (state->key_down_cb)) {
        state->key_down_cb((int)wgpu_key);
    }
    return EM_TRUE;
}

static EM_BOOL emsc_keyup_cb(int type, const EmscriptenKeyboardEvent* ev, void* userdata) {
    (void)type;
    wgpu_state_t* state = (wgpu_state_t*)userdata;
    wgpu_keycode_t wgpu_key = emsc_translate_key(ev->code);
    if ((WGPU_KEY_INVALID != wgpu_key) && (state->key_up_cb)) {
        state->key_up_cb((int)wgpu_key);
    }
    return EM_TRUE;
}

static EM_BOOL emsc_keypress_cb(int type, const EmscriptenKeyboardEvent* ev, void* userdata) {
    (void)type;
    wgpu_state_t* state = (wgpu_state_t*)userdata;
    if (state->char_cb) {
        state->char_cb(ev->charCode);
    }
    return EM_TRUE;
}

static EM_BOOL emsc_mousedown_cb(int type, const EmscriptenMouseEvent* ev, void* userdata) {
    (void)type;
    wgpu_state_t* state = (wgpu_state_t*)userdata;
    if ((ev->button < 3) && (state->mouse_btn_down_cb)) {
        state->mouse_btn_down_cb(ev->button);
    }
    return EM_TRUE;
}

static EM_BOOL emsc_mouseup_cb(int type, const EmscriptenMouseEvent* ev, void* userdata) {
    (void)type;
    wgpu_state_t* state = (wgpu_state_t*)userdata;
    if ((ev->button < 3) && (state->mouse_btn_up_cb)) {
        state->mouse_btn_up_cb(ev->button);
    }
    return EM_TRUE;
}

static EM_BOOL emsc_mousemove_cb(int type, const EmscriptenMouseEvent* ev, void* userdata) {
    (void)type;
    wgpu_state_t* state = (wgpu_state_t*)userdata;
    if (state->mouse_pos_cb) {
        state->mouse_pos_cb((float) ev->targetX, (float)ev->targetY);
    }
    return EM_TRUE;
}

static EM_BOOL emsc_wheel_cb(int type, const EmscriptenWheelEvent* ev, void* userdata) {
    (void)type;
    wgpu_state_t* state = (wgpu_state_t*)userdata;
    if (state->mouse_wheel_cb) {
        state->mouse_wheel_cb(-0.1f * (float)ev->deltaY);
    }
    return EM_TRUE;
}

static WGPUStringView string_view(const char* str) {
    return str ? (WGPUStringView){ .data = str, .length = strlen(str) } : (WGPUStringView){0};
}

static void uncaptured_error_cb(const WGPUDevice* dev, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)dev; (void)userdata1; (void)userdata2;
    if (type != WGPUErrorType_NoError) {
        printf("UNCAPTURED ERROR: %s\n", message.data);
    }
}

static void device_lost_cb(const WGPUDevice* dev, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)dev; (void)reason; (void)userdata1; (void)userdata2;
    printf("DEVICE LOST: %s\n", message.data);
}

static void error_scope_cb(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)status; (void)userdata1; (void)userdata2;
    if (type != WGPUErrorType_NoError) {
        printf("ERROR: %s\n", message.data);
    }
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)status; (void)message; (void)userdata2;
    wgpu_state_t* state = userdata1;
    if (status != WGPURequestDeviceStatus_Success) {
        printf("wgpuAdapterRequestDevice failed with %s!\n", message.data);
        state->async_setup_failed = true;
        return;
    }
    state->device = device;

    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);

    // setup swapchain
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc = {
        .chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector,
        .selector = string_view("#canvas"),
    };
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = &canvas_desc.chain,
    };
    state->surface = wgpuInstanceCreateSurface(state->instance, &surf_desc);
    if (!state->surface) {
        printf("wgpuInstanceCreateSurface() failed.\n");
        state->async_setup_failed = true;
        return;
    }
    WGPUSurfaceCapabilities surf_caps = {0};
    wgpuSurfaceGetCapabilities(state->surface, state->adapter, &surf_caps);
    state->render_format = wgpu_pick_render_format(surf_caps.format_count, surf_caps.formats);
    wgpu_swapchain_init(state);
    state->desc.init_cb();
    wgpuDevicePopErrorScope(state->device, (WGPUPopErrorScopeCallbackInfo){
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = error_scope_cb
    });
    state->async_setup_done = true;
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)message; (void)userdata2;
    wgpu_state_t* state = userdata1;
    if (status != WGPURequestAdapterStatus_Success) {
        printf("wgpuInstanceRequestAdapter failed!\n");
        state->async_setup_failed = true;
    }
    state->adapter = adapter;

    const WGPUFeatureName requiredFeatures[1] = {
        WGPUFeatureName_Depth32FloatStencil8
    };
    wgpuAdapterRequestDevice(adapter,
        &(WGPUDeviceDescriptor){
            .requiredFeatureCount = 1,
            .requiredFeatures = requiredFeatures,
            .deviceLostCallbackInfo = {
                .mode = WGPUCallbackMode_AllowProcessEvents,
                .callback = device_lost_cb,
            },
            .uncapturedErrorCallbackInfo = {
                .callback = uncaptured_error_cb,
            },
        },
        (WGPURequestDeviceCallbackInfo){
            .mode = WGPUCallbackMode_AllowProcessEvents,
            .callback = request_device_cb,
            .userdata1 = userdata1,
            .userdata2 = userdata2,
        });
}

static EM_BOOL emsc_frame(double time, void* userdata) {
    (void)time;
    wgpu_state_t* state = userdata;
    wgpuInstanceProcessEvents(state->instance);
    if (state->async_setup_failed) {
        return EM_FALSE;
    }
    if (!state->async_setup_done) {
        return EM_TRUE;
    }
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);
    state->swapchain_view = wgpu_swapchain_next(state);
    if (state->swapchain_view) {
        state->desc.frame_cb();
        wgpuTextureViewRelease(state->swapchain_view);
        state->swapchain_view = 0;
    }
    wgpuDevicePopErrorScope(state->device, (WGPUPopErrorScopeCallbackInfo){
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = error_scope_cb
    });
    return EM_TRUE;
}

void wgpu_platform_start(wgpu_state_t* state) {
    assert(state->instance == 0);

    state->instance = wgpuCreateInstance(0);
    assert(state->instance);
    wgpuInstanceRequestAdapter(state->instance, 0, (WGPURequestAdapterCallbackInfo){
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = request_adapter_cb,
        .userdata1 = state,
    });

    emsc_update_canvas_size(state);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, false, emsc_size_changed);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, true, emsc_keydown_cb);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, true, emsc_keyup_cb);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, true, emsc_keypress_cb);
    emscripten_set_mousedown_callback("#canvas", state, true, emsc_mousedown_cb);
    emscripten_set_mouseup_callback("#canvas", state, true, emsc_mouseup_cb);
    emscripten_set_mousemove_callback("#canvas", state, true, emsc_mousemove_cb);
    emscripten_set_wheel_callback("#canvas", state, true, emsc_wheel_cb);

    emscripten_request_animation_frame_loop(emsc_frame, state);
}
