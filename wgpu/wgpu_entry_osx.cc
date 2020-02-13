/* macOS+Metal specific WGPU demo scaffold functions */
#if !defined(__APPLE__)
#error "please compile this file on macOS!"
#endif

#include "SampleUtils.h"
#include "utils/WGPUHelpers.h"
#include "utils/SystemUtils.h"
#include "third_party/glfw/include/GLFW/glfw3.h"

#include "wgpu_entry.h"

void wgpu_platform_start(const wgpu_desc_t* desc) {
    /* create device */
    wgpu_state.dev = CreateCppDawnDevice(desc->title, desc->width, desc->height).Release();

    /* setup swap chain */
    WGPUSwapChainDescriptor swap_desc = {
        .implementation = GetSwapChainImplementation()
    };
    wgpu_state.swapchain = wgpuDeviceCreateSwapChain(wgpu_state.dev, nullptr, &swap_desc);
    wgpu_state.swapchain_format = (WGPUTextureFormat) GetPreferredSwapChainTextureFormat();
    wgpuSwapChainConfigure(wgpu_state.swapchain, wgpu_state.swapchain_format, WGPUTextureUsage_OutputAttachment, desc->width, desc->height);

    /* setup default depth-stencil surface */
    WGPUTextureDescriptor ds_desc = {
        .usage = WGPUTextureUsage_OutputAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = (uint32_t) desc->width,
            .height = (uint32_t) desc->height,
            .depth = 1,
        },
        .arrayLayerCount = 1,
        .format = WGPUTextureFormat_Depth24PlusStencil8,
        .mipLevelCount = 1,
        .sampleCount = 1
    };
    wgpu_state.ds_tex = wgpuDeviceCreateTexture(wgpu_state.dev, &ds_desc);
    wgpu_state.ds_view = wgpuTextureCreateView(wgpu_state.ds_tex, 0);

    if (desc->init_cb) {
        desc->init_cb();
    }
    while (!ShouldQuit()) {
        if (desc->frame_cb) {
            desc->frame_cb();
        }
        DoFlush();
        wgpuSwapChainPresent(wgpu_state.swapchain);
        utils::USleep(16000);   // AARGH
    }
    if (desc->shutdown_cb) {
        desc->shutdown_cb();
    }
    wgpuTextureViewRelease(wgpu_state.ds_view);
    wgpu_state.ds_view = 0;
    wgpuTextureRelease(wgpu_state.ds_tex);
    wgpu_state.ds_tex = 0;
    wgpuSwapChainRelease(wgpu_state.swapchain);
    wgpu_state.swapchain = 0;
    // FIXME: this currently asserts
    //wgpuDeviceRelease(wgpu_state.dev);
    wgpu_state.dev = 0;
}
