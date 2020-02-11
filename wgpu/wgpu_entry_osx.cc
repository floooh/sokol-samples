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
    wgpu_state.dev = CreateCppDawnDevice(desc->title, desc->width, desc->height).Release();
    WGPUSwapChainDescriptor swap_desc = {
        .implementation = GetSwapChainImplementation()
    };
    wgpu_state.swap = wgpuDeviceCreateSwapChain(wgpu_state.dev, nullptr, &swap_desc);
    wgpu_state.swap_fmt = (WGPUTextureFormat) GetPreferredSwapChainTextureFormat();
    wgpuSwapChainConfigure(wgpu_state.swap, wgpu_state.swap_fmt, WGPUTextureUsage_OutputAttachment, desc->width, desc->height);
    if (desc->init_cb) {
        desc->init_cb((const void*)wgpu_state.dev, (const void*)wgpu_state.swap);
    }
    while (!ShouldQuit()) {
        if (desc->frame_cb) {
            desc->frame_cb();
        }
        DoFlush();
        wgpuSwapChainPresent(wgpu_state.swap);
        utils::USleep(16000);   // AARGH
    }
    if (desc->shutdown_cb) {
        desc->shutdown_cb();
    }
}
