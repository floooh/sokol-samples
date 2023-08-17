/* platform-agnostic WGPU demo scaffold functions */

#if !defined(__EMSCRIPTEN__)
#include "GLFW/glfw3.h"
#endif

#include "sokol_gfx.h"
#include "wgpu_entry.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

static wgpu_state_t state;

#define wgpu_def(val, def) ((val==0)?def:val)

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, const char* msg, void* userdata) {
    (void)status; (void)msg; (void)userdata;
    state.device = device;
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* msg, void* userdata) {
    (void)msg;
    if (status != WGPURequestAdapterStatus_Success) {
        printf("wgpuInstanceRequestAdapter failed!\n");
        exit(10);
    }
    state.adapter = adapter;
    wgpuAdapterRequestDevice(adapter, 0, request_device_cb, userdata);
    assert(0 != state.device);
}

static void error_cb(WGPUErrorType type, const char* message, void* userdata) {
    (void)type; (void)userdata;
    if (type != WGPUErrorType_NoError) {
        printf("ERROR: %s\n", message);
    }
}

static void logging_cb(WGPULoggingType type, const char* message, void* userdata) {
    (void)type; (void)userdata;
    printf("LOG: %s\n", message);
}

static void swapchain_init(WGPUSurface surface) {
    assert(0 == state.swapchain);
    assert(0 == state.depth_stencil_tex);
    assert(0 == state.depth_stencil_view);
    assert(0 == state.msaa_tex);
    assert(0 == state.msaa_view);

    state.swapchain = wgpuDeviceCreateSwapChain(state.device, surface, &(WGPUSwapChainDescriptor){
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = state.render_format,
        .width = (uint32_t)state.width,
        .height = (uint32_t)state.height,
        .presentMode = WGPUPresentMode_Fifo,
    });
    assert(state.swapchain);

    state.depth_stencil_tex = wgpuDeviceCreateTexture(state.device, &(WGPUTextureDescriptor){
        .usage = WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = (uint32_t) state.width,
            .height = (uint32_t) state.height,
            .depthOrArrayLayers = 1,
        },
        .format = WGPUTextureFormat_Depth24PlusStencil8,
        .mipLevelCount = 1,
        .sampleCount = (uint32_t)state.desc.sample_count
    });
    assert(state.depth_stencil_tex);
    state.depth_stencil_view = wgpuTextureCreateView(state.depth_stencil_tex, 0);
    assert(state.depth_stencil_view);

    if (state.desc.sample_count > 1) {
        state.msaa_tex = wgpuDeviceCreateTexture(state.device, &(WGPUTextureDescriptor){
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = (uint32_t) state.width,
                .height = (uint32_t) state.height,
                .depthOrArrayLayers = 1,
            },
            .format = state.render_format,
            .mipLevelCount = 1,
            .sampleCount = (uint32_t)state.desc.sample_count,
        });
        assert(state.msaa_tex);
        state.msaa_view = wgpuTextureCreateView(state.msaa_tex, 0);
        assert(state.msaa_view);
    }
}

static void swapchain_discard(void) {
    if (state.msaa_view) {
        wgpuTextureViewRelease(state.msaa_view);
        state.msaa_view = 0;
    }
    if (state.msaa_tex) {
        wgpuTextureRelease(state.msaa_tex);
        state.msaa_tex = 0;
    }
    if (state.depth_stencil_view) {
        wgpuTextureViewRelease(state.depth_stencil_view);
        state.depth_stencil_view = 0;
    }
    if (state.depth_stencil_tex) {
        wgpuTextureRelease(state.depth_stencil_tex);
        state.depth_stencil_tex = 0;
    }
    if (state.swapchain) {
        wgpuSwapChainRelease(state.swapchain);
        state.swapchain = 0;
    }
}

void wgpu_start(const wgpu_desc_t* desc) {
    assert(desc);
    assert(desc->title);
    assert((desc->width > 0) && (desc->height > 0));
    assert(desc->init_cb && desc->frame_cb && desc->shutdown_cb);
    state.desc = *desc;
    state.desc.sample_count = wgpu_def(state.desc.sample_count, 1);
    state.render_format = WGPUTextureFormat_BGRA8Unorm;
    state.width = state.desc.width;
    state.height = state.desc.height;

    state.instance = wgpuCreateInstance(0);
    assert(state.instance);
    wgpuInstanceRequestAdapter(state.instance, 0, request_adapter_cb, 0);
    assert(state.device);

    wgpuDeviceSetUncapturedErrorCallback(state.device, error_cb, 0);
    wgpuDeviceSetLoggingCallback(state.device, logging_cb, 0);
    wgpuDeviceSetDeviceLostCallback(state.device, 0, 0);
    wgpuDevicePushErrorScope(state.device, WGPUErrorFilter_Validation);

    #if !__EMSCRIPTEN__
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(state.width, state.height, state.desc.title, 0, 0);
    const WGPUSurface surface = wgpu_glfw_create_surface_for_window(state.instance, window);
    assert(surface);
    #endif
    swapchain_init(surface);
    state.desc.init_cb();
    wgpuDevicePopErrorScope(state.device, error_cb, 0);
    wgpuInstanceProcessEvents(state.instance);

    #if !__EMSCRIPTEN__
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        wgpuDevicePushErrorScope(state.device, WGPUErrorFilter_Validation);
        state.swapchain_view = wgpuSwapChainGetCurrentTextureView(state.swapchain);
        state.desc.frame_cb();
        wgpuSwapChainPresent(state.swapchain);
        wgpuTextureViewRelease(state.swapchain_view);
        state.swapchain_view = 0;
        wgpuDevicePopErrorScope(state.device, error_cb, 0);
        wgpuInstanceProcessEvents(state.instance);
    }
    #endif
    state.desc.shutdown_cb();
    swapchain_discard();
    wgpuDeviceRelease(state.device);
    wgpuAdapterRelease(state.adapter);
    wgpuInstanceRelease(state.instance);
}

int wgpu_width(void) {
    return state.width;
}

int wgpu_height(void) {
    return state.height;
}

static const void* wgpu_get_render_view(void* user_data) {
    assert((void*)0xABADF00D == user_data); (void)user_data;
    if (state.desc.sample_count > 1) {
        assert(state.msaa_view);
        return (const void*) state.msaa_view;
    } else {
        assert(state.swapchain_view);
        return (const void*) state.swapchain_view;
    }
}

static const void* wgpu_get_resolve_view(void* user_data) {
    assert((void*)0xABADF00D == user_data); (void)user_data;
    if (state.desc.sample_count > 1) {
        assert(state.swapchain_view);
        return (const void*) state.swapchain_view;
    } else {
        return 0;
    }
}

static const void* wgpu_get_depth_stencil_view(void* user_data) {
    assert((void*)0xABADF00D == user_data); (void)user_data;
    return (const void*) state.depth_stencil_view;
}

static sg_pixel_format wgpu_get_color_format(void) {
    switch (state.render_format) {
        case WGPUTextureFormat_RGBA8Unorm:  return SG_PIXELFORMAT_RGBA8;
        case WGPUTextureFormat_BGRA8Unorm:  return SG_PIXELFORMAT_BGRA8;
        default: return SG_PIXELFORMAT_NONE;
    }
}

sg_context_desc wgpu_get_context(void) {
    return (sg_context_desc) {
        .color_format = wgpu_get_color_format(),
        .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .sample_count = state.desc.sample_count,
        .wgpu = {
            .device = (const void*) state.device,
            .render_view_userdata_cb = wgpu_get_render_view,
            .resolve_view_userdata_cb = wgpu_get_resolve_view,
            .depth_stencil_view_userdata_cb = wgpu_get_depth_stencil_view,
            .user_data = (void*)0xABADF00D
        }
    };
}
