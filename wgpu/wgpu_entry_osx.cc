/* macOS+Metal specific WGPU demo scaffold functions */
#if !defined(__APPLE__)
#error "please compile this file on macOS!"
#endif

#include "SampleUtils.h"
#include "utils/WGPUHelpers.h"

#include "wgpu_entry.h"


void wgpu_platform_start(const wgpu_desc_t* desc) {
    wgpu_state.dev = CreateCppDawnDevice().Release();
    WGPUSwapChainDescriptor swap_desc = {
        .implementation = GetSwapChainImplementation()
    };
    wgpu_state.swap = wgpuDeviceCreateSwapChain(wgpu_state.dev, nullptr, &swap_desc);

}
