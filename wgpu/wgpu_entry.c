/* platform-agnostic WGPU demo scaffold functions */

#include "wgpu_entry.h"
#include <assert.h>

wgpu_state_t wgpu;

void wgpu_start(const wgpu_desc_t* desc) {
    assert(desc);
    wgpu.desc = *desc;
    wgpu_platform_start(desc);
}

int wgpu_width(void) {
    return wgpu.width;
}

int wgpu_height(void) {
    return wgpu.height;
}
