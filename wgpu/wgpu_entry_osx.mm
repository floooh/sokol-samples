/* macOS+Metal specific WGPU demo scaffold functions */
#if !defined(__APPLE__)
#error "please compile this file on macOS!"
#endif

#include "SampleUtils.h"
#include "utils/WGPUHelpers.h"
#include "utils/SystemUtils.h"
#include "third_party/glfw/include/GLFW/glfw3.h"

#include "sokol_gfx.h"
#include "wgpu_entry.h"

void wgpu_platform_start(const wgpu_desc_t* desc) {

    wgpu_state.width = desc->width;
    wgpu_state.height = desc->height;

    /* create device */
    wgpu_state.dev = CreateCppDawnDevice(desc->title, wgpu_state.width, wgpu_state.height).Release();

    /* setup swap chain */
    WGPUSwapChainDescriptor swap_desc = {
        .implementation = GetSwapChainImplementation(),
        .presentMode = WGPUPresentMode_Fifo
    };
    wgpu_state.swapchain = wgpuDeviceCreateSwapChain(wgpu_state.dev, nullptr, &swap_desc);
    wgpu_state.render_format = (WGPUTextureFormat) GetPreferredSwapChainTextureFormat();
    wgpuSwapChainConfigure(wgpu_state.swapchain, wgpu_state.render_format, WGPUTextureUsage_OutputAttachment, desc->width, desc->height);

    /* setup the swapchain surfaces */
    wgpu_swapchain_init();

    /* application init and frame loop */
    desc->init_cb();
    while (!ShouldQuit()) {
        @autoreleasepool {
            wgpu_swapchain_next_frame();
            desc->frame_cb();
            DoFlush();
            wgpuSwapChainPresent(wgpu_state.swapchain);
        }
    }

    /* shutdown everything */
    desc->shutdown_cb();
    wgpu_swapchain_discard();
    wgpuSwapChainRelease(wgpu_state.swapchain);
    wgpu_state.swapchain = 0;
    // FIXME: this currently asserts
    //wgpuDeviceRelease(wgpu_state.dev);
    wgpu_state.dev = 0;
}
