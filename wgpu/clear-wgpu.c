//------------------------------------------------------------------------------
//  clear-wgpu.c
//  Simple draw loop, clear default framebuffer.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "wgpu_entry.h"

static sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.0f, 0.0f, 1.0f } }
};

static void init(void) {
    sg_setup(&(sg_desc){
        .context = wgpu_get_context()
    });
}

static void frame(void) {
    /* animate clear colors */
    float g = pass_action.colors[0].value.g + 0.01f;
    if (g > 1.0f) g = 0.0f;
    pass_action.colors[0].value.g = g;

    /* draw one frame */
    sg_begin_default_pass(&pass_action, wgpu_width(), wgpu_height());
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
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
