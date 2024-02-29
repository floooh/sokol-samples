//------------------------------------------------------------------------------
//  wgpu_entry_swapchain.c
//
//  Swapchain helper functions.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#include <assert.h>

void wgpu_swapchain_init(wgpu_state_t* state) {
    assert(state->adapter);
    assert(state->device);
    assert(state->surface);
    assert(state->render_format != WGPUTextureFormat_Undefined);
    assert(0 == state->swapchain);
    assert(0 == state->depth_stencil_tex);
    assert(0 == state->depth_stencil_view);
    assert(0 == state->msaa_tex);
    assert(0 == state->msaa_view);

    state->swapchain = wgpuDeviceCreateSwapChain(state->device, state->surface, &(WGPUSwapChainDescriptor){
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = state->render_format,
        .width = (uint32_t)state->width,
        .height = (uint32_t)state->height,
        .presentMode = WGPUPresentMode_Fifo,
    });
    assert(state->swapchain);

    if (!state->desc.no_depth_buffer) {
        state->depth_stencil_tex = wgpuDeviceCreateTexture(state->device, &(WGPUTextureDescriptor){
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = (uint32_t) state->width,
                .height = (uint32_t) state->height,
                .depthOrArrayLayers = 1,
            },
            .format = WGPUTextureFormat_Depth32FloatStencil8,
            .mipLevelCount = 1,
            .sampleCount = (uint32_t)state->desc.sample_count
        });
        assert(state->depth_stencil_tex);
        state->depth_stencil_view = wgpuTextureCreateView(state->depth_stencil_tex, 0);
        assert(state->depth_stencil_view);
    }

    if (state->desc.sample_count > 1) {
        state->msaa_tex = wgpuDeviceCreateTexture(state->device, &(WGPUTextureDescriptor){
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = (uint32_t) state->width,
                .height = (uint32_t) state->height,
                .depthOrArrayLayers = 1,
            },
            .format = state->render_format,
            .mipLevelCount = 1,
            .sampleCount = (uint32_t)state->desc.sample_count,
        });
        assert(state->msaa_tex);
        state->msaa_view = wgpuTextureCreateView(state->msaa_tex, 0);
        assert(state->msaa_view);
    }
}

void wgpu_swapchain_discard(wgpu_state_t* state) {
    if (state->msaa_view) {
        wgpuTextureViewRelease(state->msaa_view);
        state->msaa_view = 0;
    }
    if (state->msaa_tex) {
        wgpuTextureRelease(state->msaa_tex);
        state->msaa_tex = 0;
    }
    if (state->depth_stencil_view) {
        wgpuTextureViewRelease(state->depth_stencil_view);
        state->depth_stencil_view = 0;
    }
    if (state->depth_stencil_tex) {
        wgpuTextureRelease(state->depth_stencil_tex);
        state->depth_stencil_tex = 0;
    }
    if (state->swapchain) {
        wgpuSwapChainRelease(state->swapchain);
        state->swapchain = 0;
    }
}
