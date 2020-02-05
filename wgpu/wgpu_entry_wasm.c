/* WASM specific WGPU demo scaffold functions */
#if !defined(__EMSCRIPTEN__)
#error "please compile this file in Emscripten!"
#endif

#include <stdio.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include "wgpu_entry.h"

static struct {
    int state_count;
} emsc;

static void emsc_update_canvas_size(void) {
    double w, h;
    emscripten_get_element_css_size("canvas", &w, &h);
    emscripten_set_canvas_element_size("canvas", w, h);
    wgpu.width = (int) w;
    wgpu.height = (int) h;
    printf("canvas size updated: %d %d\n", wgpu.width, wgpu.height);
}

static EM_BOOL emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    emsc_update_canvas_size();
    return true;
}

EMSCRIPTEN_KEEPALIVE void emsc_device_ready(int device_id, int swapchain_id) {
    wgpu.dev = (WGPUDevice) device_id;
    wgpu.swap = (WGPUSwapChain) swapchain_id;
}

EM_JS(void, emsc_js_setup, (), {
    WebGPU.initManagers();
    navigator.gpu.requestAdapter().then(function(adapter) {
        adapter.requestDevice().then(function(device) {
            const gpuContext = document.getElementById("canvas").getContext("gpupresent");
            // FIXME: getSwapChainPreferredFormat
            const swapChainDescriptor = { device: device, format: "bgra8unorm" };
            const swapChain = gpuContext.configureSwapChain(swapChainDescriptor);
            console.log("device: " + device);
            console.log("swap chain: " + swapChain);
            var deviceId = WebGPU.mgrDevice.create(device);
            var swapChainId = WebGPU.mgrSwapChain.create(swapChain);
            _emsc_device_ready(deviceId, swapChainId);
        });
    });
});

static void emsc_post_setup(void) {
    wgpuDeviceReference(wgpu.dev);
    wgpuSwapChainReference(wgpu.swap);
}

static EM_BOOL emsc_frame(double time, void* user_data) {
    switch (emsc.state_count) {
        case 0:
            emsc_js_setup();
            emsc.state_count = 1;
            break;
        case 1:
            if (wgpu.dev) {
                emsc_post_setup();
                wgpu.desc.init_cb((const void*)wgpu.dev, (const void*)wgpu.swap);
                emsc.state_count = 2;
            }
            break;
        case 2:
            wgpu.desc.frame_cb();
            break;
    }
    return EM_TRUE;
}

void wgpu_platform_start(const wgpu_desc_t* desc) {
    emsc_update_canvas_size();
    emscripten_set_resize_callback("canvas", 0, false, emsc_size_changed);
    emsc_js_setup();
    emscripten_request_animation_frame_loop(emsc_frame, 0);
}
