/* platform-agnostic WGPU demo scaffold functions */

#include "wgpu_entry.h"
#include <assert.h>

wgpu_state_t wgpu_state;

void wgpu_start(const wgpu_desc_t* desc) {
    assert(desc);
    assert(desc->title);
    assert((desc->width > 0) && (desc->height > 0));
    assert(desc->init_cb && desc->frame_cb && desc->shutdown_cb);
    wgpu_state.desc = *desc;
    wgpu_platform_start(desc);
}

int wgpu_width(void) {
    return wgpu_state.width;
}

int wgpu_height(void) {
    return wgpu_state.height;
}

const void* wgpu_device(void) {
    return (const void*) wgpu_state.dev;
}

const void* wgpu_swapchain(void) {
    return (const void*) wgpu_state.swapchain;
}

const void* wgpu_depth_stencil_view(void) {
    return (const void*) wgpu_state.ds_view;
}

uint32_t wgpu_swapchain_format(void) {
    return (uint32_t) wgpu_state.swapchain_format;
}

void wgpu_create_default_depth_stencil_surface(void) {
    assert((wgpu_state.width > 0) && (wgpu_state.height > 0));
    WGPUTextureDescriptor ds_desc = {
        .usage = WGPUTextureUsage_OutputAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = (uint32_t) wgpu_state.width,
            .height = (uint32_t) wgpu_state.height,
            .depth = 1,
        },
        .arrayLayerCount = 1,
        .format = WGPUTextureFormat_Depth24PlusStencil8,
        .mipLevelCount = 1,
        .sampleCount = 1
    };
    wgpu_state.ds_tex = wgpuDeviceCreateTexture(wgpu_state.dev, &ds_desc);
    wgpu_state.ds_view = wgpuTextureCreateView(wgpu_state.ds_tex, 0);
}

void wgpu_discard_default_depth_stencil_surface(void) {
    wgpuTextureViewRelease(wgpu_state.ds_view);
    wgpu_state.ds_view = 0;
    wgpuTextureRelease(wgpu_state.ds_tex);
    wgpu_state.ds_tex = 0;
}
