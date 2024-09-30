//------------------------------------------------------------------------------
//  wgpu_entry_swapchain.c
//
//  Swapchain helper functions.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void wgpu_swapchain_init(wgpu_state_t* state) {
    assert(state->adapter);
    assert(state->device);
    assert(state->surface);
    assert(state->render_format != WGPUTextureFormat_Undefined);
    assert(0 == state->depth_stencil_tex);
    assert(0 == state->depth_stencil_view);
    assert(0 == state->msaa_tex);
    assert(0 == state->msaa_view);

    wgpuSurfaceConfigure(state->surface, &(WGPUSurfaceConfiguration){
        .device = state->device,
        .format = state->render_format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .width = (uint32_t)state->width,
        .height = (uint32_t)state->height,
        .presentMode = WGPUPresentMode_Fifo,
    });

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
}

void wgpu_swapchain_resized(wgpu_state_t* state) {
    wgpu_swapchain_discard(state);
    wgpu_swapchain_init(state);
}

// may return 0, in that case: skip this frame
WGPUTextureView wgpu_swapchain_next(wgpu_state_t* state) {
    WGPUSurfaceTexture surface_texture = {0};
    wgpuSurfaceGetCurrentTexture(state->surface, &surface_texture);
    switch (surface_texture.status) {
        case WGPUSurfaceGetCurrentTextureStatus_Success:
            // all ok
            break;
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
            // skip this frame and reconfigure surface
            if (surface_texture.texture) {
                wgpuTextureRelease(surface_texture.texture);
            }
            wgpu_swapchain_discard(state);
            wgpu_swapchain_init(state);
            return 0;
        case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
        case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        default:
            printf("wgpuSurfaceGetCurrentTexture() failed with: %#.8x\n", surface_texture.status);
            abort();
    }
    WGPUTextureView view = wgpuTextureCreateView(surface_texture.texture, 0);
    wgpuTextureRelease(surface_texture.texture);
    return view;
}
