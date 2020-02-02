#pragma once
/* Canvas + WebGPU initialization via emscripten */
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

typedef struct {
    void (*init_cb)(const void* wgpu_device, const void* wgpu_swapchain);
    void (*frame_cb)(void);
} emsc_desc_t;

static struct {
    emsc_desc_t desc;
    int state_count;
    double canvas_width;
    double canvas_height;
    WGPUDevice device;
    WGPUSwapChain swap_chain;
} emsc_state;

/* track CSS element size changes and update the WebGL canvas size */
static EM_BOOL emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    emscripten_get_element_css_size("canvas", &emsc_state.canvas_width, &emsc_state.canvas_height);
    emscripten_set_canvas_element_size("canvas", emsc_state.canvas_width, emsc_state.canvas_height);
    printf("canvas size changed: %d %d\n", (int)emsc_state.canvas_width, (int)emsc_state.canvas_height);
    return true;
}

EMSCRIPTEN_KEEPALIVE void emsc_wgpu_device_ready(int device_id, int swapchain_id) {
    emsc_state.device = (WGPUDevice) device_id;
    emsc_state.swap_chain = (WGPUSwapChain) swapchain_id;
}

EM_JS(void, js_wgpu_start, (), {
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
            _emsc_wgpu_device_ready(deviceId, swapChainId);
        });
    });
});

void wgpu_init() {
    wgpuDeviceReference(emsc_state.device);
    wgpuSwapChainReference(emsc_state.swap_chain);
}

static EM_BOOL emsc_frame(double time, void* user_data) {
    switch (emsc_state.state_count) {
        case 0:
            puts("setup WGPU device...");
            js_wgpu_start();
            emsc_state.state_count = 1;
            break;
        case 1:
            if (emsc_state.device) {
                printf("WGPU device ready: %d\nWGPU swap chain: %d\n", (int)emsc_state.device, (int)emsc_state.swap_chain);
                wgpu_init();
                emsc_state.desc.init_cb((const void*)emsc_state.device, (const void*)emsc_state.swap_chain);
                emsc_state.state_count = 2;
            }
            break;
        case 2:
            emsc_state.desc.frame_cb();
            break;
    }
    return EM_TRUE;
}

/* initialize WebGPU and canvas */
static void emsc_init(const emsc_desc_t* desc) {
    assert(desc && desc->init_cb && desc->frame_cb);

    emsc_state.desc = *desc;
    emscripten_get_element_css_size("canvas", &emsc_state.canvas_width, &emsc_state.canvas_height);
    emscripten_set_canvas_element_size("canvas", emsc_state.canvas_width, emsc_state.canvas_height);
    emscripten_set_resize_callback("canvas", 0, false, emsc_size_changed);
    printf("canvas size: %d %d\n", (int)emsc_state.canvas_width, (int)emsc_state.canvas_height);

    emscripten_request_animation_frame_loop(emsc_frame, 0);
}

static int emsc_width() {
    return (int) emsc_state.canvas_width;
}

static int emsc_height() {
    return (int) emsc_state.canvas_height;
}
