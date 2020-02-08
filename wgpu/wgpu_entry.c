/* platform-agnostic WGPU demo scaffold functions */

#include "wgpu_entry.h"
#include <assert.h>

wgpu_state_t wgpu_state;

void wgpu_start(const wgpu_desc_t* desc) {
    assert(desc);
    assert(desc->title);
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
