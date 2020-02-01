//------------------------------------------------------------------------------
//  clear-wgpu.c
//  Simple draw loop, clear default framebuffer.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "emsc.h"

static sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
};

void init(const void* wgpu_device, const void* wgpu_swap_chain) {
    sg_setup(&(sg_desc){
        .wgpu_device = wgpu_device,
        .wgpu_swap_chain = wgpu_swap_chain,
    });
    printf("sg_setup() called\n");
}

void frame(void) {
    /* animate clear colors */
    float g = pass_action.colors[0].val[1] + 0.01f;
    if (g > 1.0f) g = 0.0f;
    pass_action.colors[0].val[1] = g;

    /* draw one frame */
    sg_begin_default_pass(&pass_action, emsc_width(), emsc_height());
    sg_end_pass();
    sg_commit();
}

int main() {
    emsc_init(&(emsc_desc_t){
        .init_cb = init,
        .frame_cb = frame
    });
    return 0;
}
