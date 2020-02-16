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

    wgpu_state.width = desc->width;
    wgpu_state.height = desc->height;

    /* create device */
    wgpu_state.dev = CreateCppDawnDevice(desc->title, wgpu_state.width, wgpu_state.height).Release();

    /* setup swap chain */
    WGPUSwapChainDescriptor swap_desc = {
        .implementation = GetSwapChainImplementation()
    };
    wgpu_state.swapchain = wgpuDeviceCreateSwapChain(wgpu_state.dev, nullptr, &swap_desc);
    wgpu_state.swapchain_format = (WGPUTextureFormat) GetPreferredSwapChainTextureFormat();
    wgpuSwapChainConfigure(wgpu_state.swapchain, wgpu_state.swapchain_format, WGPUTextureUsage_OutputAttachment, desc->width, desc->height);

    /* setup the default depth-stencil surface */
    wgpu_create_default_depth_stencil_surface();

    /* application init and frame loop */
    desc->init_cb();
    while (!ShouldQuit()) {
        @autoreleasepool {
            desc->frame_cb();
            DoFlush();
            wgpuSwapChainPresent(wgpu_state.swapchain);
            utils::USleep(16000);   // AARGH
        }
    }

    /* shutdown everythind */
    desc->shutdown_cb();
    wgpu_discard_default_depth_stencil_surface();
    wgpuSwapChainRelease(wgpu_state.swapchain);
    wgpu_state.swapchain = 0;
    // FIXME: this currently asserts
    //wgpuDeviceRelease(wgpu_state.dev);
    wgpu_state.dev = 0;
}
