//------------------------------------------------------------------------------
//  wgpu_entry_emsc.c
//  Emscripten-specific WebGPU scaffolding. NOTE: does not scale the swapchain
//  surfaces when the window size changes.
//------------------------------------------------------------------------------
#include <stdio.h>
#include <assert.h>
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
    return true;
}

static void error_cb(WGPUErrorType type, const char* message, void* userdata) {
    (void)type; (void)userdata;
    if (type != WGPUErrorType_NoError) {
        printf("ERROR: %s\n", message);
    }
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, const char* msg, void* userdata) {
    (void)status; (void)msg; (void)userdata;
    wgpu_state_t* state = userdata;
    if (status != WGPURequestDeviceStatus_Success) {
        printf("wgpuAdapterRequestDevice failed with %s!\n", msg);
        state->async_setup_failed = true;
        return;
    }
    state->device = device;

    wgpuDeviceSetUncapturedErrorCallback(state->device, error_cb, 0);
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);

    // setup swapchain
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {
        .chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
        .selector = "#canvas",
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
    state->render_format = wgpuSurfaceGetPreferredFormat(state->surface, state->adapter);
    wgpu_swapchain_init(state);
    state->desc.init_cb();
    wgpuDevicePopErrorScope(state->device, error_cb, 0);
    state->async_setup_done = true;
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* msg, void* userdata) {
    (void)msg;
    wgpu_state_t* state = userdata;
    if (status != WGPURequestAdapterStatus_Success) {
        printf("wgpuInstanceRequestAdapter failed!\n");
        state->async_setup_failed = true;
    }
    state->adapter = adapter;
    wgpuAdapterRequestDevice(adapter, 0, request_device_cb, userdata);
}

static EM_BOOL emsc_frame(double time, void* userdata) {
    (void)time;
    wgpu_state_t* state = userdata;
    if (state->async_setup_failed) {
        return EM_FALSE;
    }
    if (!state->async_setup_done) {
        return EM_TRUE;
    }
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);
    state->swapchain_view = wgpuSwapChainGetCurrentTextureView(state->swapchain);
    state->desc.frame_cb();
    wgpuTextureViewRelease(state->swapchain_view);
    state->swapchain_view = 0;
    wgpuDevicePopErrorScope(state->device, error_cb, 0);
    return EM_TRUE;
}

void wgpu_platform_start(wgpu_state_t* state) {
    assert(state->instance == 0);

    emsc_update_canvas_size(state);
    emscripten_set_resize_callback("canvas", 0, false, emsc_size_changed);

    state->instance = wgpuCreateInstance(0);
    assert(state->instance);
    wgpuInstanceRequestAdapter(state->instance, 0, request_adapter_cb, state);

    emscripten_request_animation_frame_loop(emsc_frame, state);
}
