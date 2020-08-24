/* platform-agnostic WGPU demo scaffold functions */

#include "sokol_gfx.h"
#include "wgpu_entry.h"
#include <assert.h>

wgpu_state_t wgpu_state;

#define wgpu_def(val, def) ((val==0)?def:val)

void wgpu_start(const wgpu_desc_t* desc) {
    assert(desc);
    assert(desc->title);
    assert((desc->width > 0) && (desc->height > 0));
    assert(desc->init_cb && desc->frame_cb && desc->shutdown_cb);
    wgpu_state.desc = *desc;
    wgpu_state.desc.sample_count = wgpu_def(wgpu_state.desc.sample_count, 1);
    wgpu_platform_start(desc);
}

int wgpu_width(void) {
    return wgpu_state.width;
}

int wgpu_height(void) {
    return wgpu_state.height;
}

static const void* wgpu_get_render_view(void* user_data) {
    assert((void*)0xABADF00D == user_data);
    if (wgpu_state.desc.sample_count > 1) {
        assert(wgpu_state.msaa_view);
        return (const void*) wgpu_state.msaa_view;
    }
    else {
        assert(wgpu_state.swapchain_view);
        return (const void*) wgpu_state.swapchain_view;
    }
}

static const void* wgpu_get_resolve_view(void* user_data) {
    assert((void*)0xABADF00D == user_data);
    if (wgpu_state.desc.sample_count > 1) {
        assert(wgpu_state.swapchain_view);
        return (const void*) wgpu_state.swapchain_view;
    }
    else {
        return 0;
    }
}

static const void* wgpu_get_depth_stencil_view(void* user_data) {
    assert((void*)0xABADF00D == user_data);
    return (const void*) wgpu_state.depth_stencil_view;
}

static sg_pixel_format wgpu_get_color_format(void) {
    switch (wgpu_state.render_format) {
            case WGPUTextureFormat_RGBA8Unorm:  return SG_PIXELFORMAT_RGBA8;
            case WGPUTextureFormat_BGRA8Unorm:  return SG_PIXELFORMAT_BGRA8;
            /* this shouldn't happen */
            default: return SG_PIXELFORMAT_NONE;
    }
}

sg_context_desc wgpu_get_context(void) {
    return (sg_context_desc) {
        .color_format = wgpu_get_color_format(),
        .sample_count = wgpu_state.desc.sample_count,
        .wgpu = {
            .device = (const void*) wgpu_state.dev,
            .render_view_userdata_cb = wgpu_get_render_view,
            .resolve_view_userdata_cb = wgpu_get_resolve_view,
            .depth_stencil_view_userdata_cb = wgpu_get_depth_stencil_view,
            .user_data = 0xABADF00D
        }
    };
}

void wgpu_swapchain_init(void) {
    assert(wgpu_state.swapchain);
    assert((wgpu_state.width > 0) && (wgpu_state.height > 0));

    /* create depth-stencil texture and view */
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
        .sampleCount = wgpu_state.desc.sample_count
    };
    wgpu_state.depth_stencil_tex = wgpuDeviceCreateTexture(wgpu_state.dev, &ds_desc);
    wgpu_state.depth_stencil_view = wgpuTextureCreateView(wgpu_state.depth_stencil_tex, 0);

    /* create optional MSAA surface and view */
    if (wgpu_state.desc.sample_count > 1) {
        WGPUTextureDescriptor msaa_desc = {
            .usage = WGPUTextureUsage_OutputAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = (uint32_t) wgpu_state.width,
                .height = (uint32_t) wgpu_state.height,
                .depth = 1,
            },
            .arrayLayerCount = 1,
            .format = wgpu_state.render_format,
            .mipLevelCount = 1,
            .sampleCount = wgpu_state.desc.sample_count
        };
        wgpu_state.msaa_tex = wgpuDeviceCreateTexture(wgpu_state.dev, &msaa_desc);
        wgpu_state.msaa_view = wgpuTextureCreateView(wgpu_state.msaa_tex, 0);
    }
}

void wgpu_swapchain_next_frame(void) {
    if (wgpu_state.swapchain_view) {
        wgpuTextureViewRelease(wgpu_state.swapchain_view);
    }
    wgpu_state.swapchain_view = wgpuSwapChainGetCurrentTextureView(wgpu_state.swapchain);
}

void wgpu_swapchain_discard(void) {
    if (wgpu_state.msaa_tex) {
        wgpuTextureRelease(wgpu_state.msaa_tex);
    }
    if (wgpu_state.msaa_view) {
        wgpuTextureViewRelease(wgpu_state.msaa_view);
    }
    wgpuTextureViewRelease(wgpu_state.swapchain_view);
    wgpuTextureViewRelease(wgpu_state.depth_stencil_view);
    wgpuTextureRelease(wgpu_state.depth_stencil_tex);
}
