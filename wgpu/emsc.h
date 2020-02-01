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
    int state;
    double canvas_width;
    double canvas_height;
    WGPUDevice device;
    WGPUSwapChain swap_chain;
} state;

/* track CSS element size changes and update the WebGL canvas size */
static EM_BOOL emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    emscripten_get_element_css_size("canvas", &state.canvas_width, &state.canvas_height);
    emscripten_set_canvas_element_size("canvas", state.canvas_width, state.canvas_height);
    printf("canvas size changed: %d %d\n", (int)state.canvas_width, (int)state.canvas_height);
    return true;
}

EMSCRIPTEN_KEEPALIVE void emsc_wgpu_device_ready(int device_id, int swapchain_id) {
    state.device = (WGPUDevice) device_id;
    state.swap_chain = (WGPUSwapChain) swapchain_id;
}

EM_JS(void, js_wgpu_start, (), {
    WebGPU.initManagers();
    navigator.gpu.requestAdapter().then(function(adapter) {
        adapter.requestDevice().then(function(device) {
            const gpuContext = document.getElementById("canvas").getContext("gpu");
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
    wgpuDeviceReference(state.device);
    wgpuSwapChainReference(state.swap_chain);
}

static void emsc_frame(void) {
    switch (state.state) {
        case 0:
            puts("setup WGPU device...");
            js_wgpu_start();
            state.state = 1;
            break;
        case 1:
            if (state.device) {
                printf("WGPU device ready: %d\nWGPU swap chain: %d\n", (int)state.device, (int)state.swap_chain);
                wgpu_init();
                state.desc.init_cb((const void*) state.device, (const void*) state.swap_chain);
                state.state = 2;
            }
            break;
        case 2:
            state.desc.frame_cb();
            wgpuSwapChainPresent(state.swap_chain);
            break;
    }
}

/* initialize WebGPU and canvas */
static void emsc_init(const emsc_desc_t* desc) {
    assert(desc && desc->init_cb && desc->frame_cb);

    state.desc = *desc;
    emscripten_get_element_css_size("canvas", &state.canvas_width, &state.canvas_height);
    emscripten_set_canvas_element_size("canvas", state.canvas_width, state.canvas_height);
    emscripten_set_resize_callback("canvas", 0, false, emsc_size_changed);
    printf("canvas size: %d %d\n", (int)state.canvas_width, (int)state.canvas_height);

    emscripten_set_main_loop(emsc_frame, 0, 1);
}

static int emsc_width() {
    return (int) state.canvas_width;
}

static int emsc_height() {
    return (int) state.canvas_height;
}
