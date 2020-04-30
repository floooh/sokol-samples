/* WASM specific WGPU demo scaffold functions */
#if !defined(__EMSCRIPTEN__)
#error "please compile this file in Emscripten!"
#endif

#include <stdio.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include "wgpu_entry.h"

static struct {
    int frame_state;
} emsc;

static void emsc_update_canvas_size(void) {
    double w, h;
    emscripten_get_element_css_size("canvas", &w, &h);
    emscripten_set_canvas_element_size("canvas", w, h);
    wgpu_state.width = (int) w;
    wgpu_state.height = (int) h;
    printf("canvas size updated: %d %d\n", wgpu_state.width, wgpu_state.height);
}

static EM_BOOL emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    emsc_update_canvas_size();
    return true;
}

EMSCRIPTEN_KEEPALIVE void emsc_device_ready(int device_id, int swapchain_id, int swapchain_fmt) {
    wgpu_state.dev = (WGPUDevice) device_id;
    wgpu_state.swapchain = (WGPUSwapChain) swapchain_id;
    wgpu_state.render_format = (WGPUTextureFormat) swapchain_fmt;
}

/* Javascript function to wrap asynchronous device and swapchain setup */
EM_JS(void, emsc_async_js_setup, (), {
    WebGPU.initManagers();
    navigator.gpu.requestAdapter().then(function(adapter) {
        console.log("adapter extensions: " + adapter.extensions);
        adapter.requestDevice().then(function(device) {
            console.log("device extensions: " + device.extensions);
            var gpuContext = document.getElementById("canvas").getContext("gpupresent");
            gpuContext.getSwapChainPreferredFormat(device).then(function(fmt) {
                var swapChainDescriptor = { device: device, format: fmt };
                var swapChain = gpuContext.configureSwapChain(swapChainDescriptor);
                var deviceId = WebGPU.mgrDevice.create(device);
                var swapChainId = WebGPU.mgrSwapChain.create(swapChain);
                var fmtId = WebGPU.TextureFormat.findIndex(function(elm) { return elm==fmt; });
                console.log("device: " + device);
                console.log("swap chain: " + swapChain);
                console.log("preferred format: " + fmt + " (" + fmtId + ")");
                _emsc_device_ready(deviceId, swapChainId, fmtId);
            });
        });
    });
});

static EM_BOOL emsc_frame(double time, void* user_data) {
    switch (emsc.frame_state) {
        case 0:
            emsc_async_js_setup();
            emsc.frame_state = 1;
            break;
        case 1:
            if (wgpu_state.dev) {
                wgpu_swapchain_init();
                wgpu_state.desc.init_cb();
                emsc.frame_state = 2;
            }
            break;
        case 2:
            wgpu_swapchain_next_frame();
            wgpu_state.desc.frame_cb();
            break;
    }
    return EM_TRUE;
}

void wgpu_platform_start(const wgpu_desc_t* desc) {
    emsc_update_canvas_size();
    emscripten_set_resize_callback("canvas", 0, false, emsc_size_changed);
    emscripten_request_animation_frame_loop(emsc_frame, 0);
}
