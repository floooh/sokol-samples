//------------------------------------------------------------------------------
//  clear-wgpu.c
//  Simple draw loop, clear default framebuffer.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"

static void init(void) {
    // FIXME
}

static void frame(void) {
    // FIXME
}

static void shutdown(void) {
    // FIXME
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .width = 640,
        .height = 480,
        .title = "clear-wgpu"
    });
    return 0;
}
