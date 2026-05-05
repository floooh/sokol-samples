//------------------------------------------------------------------------------
//  framebuffer-sapp.c
//
//  Simple sokol_framebuffer.h test/sample.
//  Plasma effect taken from shadertoy https://www.shadertoy.com/view/MdXGDH
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#define SOKOL_FRAMEBUFFER_IMPL
#include "sokol_framebuffer.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "dbgui/dbgui.h"

#define FB_WIDTH (320)
#define FB_HEIGHT (256)

static struct {
    sfb_framebuffer fb;
    float time;
} state;

static uint32_t pixels[FB_HEIGHT][FB_WIDTH];

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    sfb_setup(&(sfb_desc){
        .logger.func = slog_func,
    });

    state.fb = sfb_make_framebuffer(&(sfb_framebuffer_desc){
        .width = FB_WIDTH,
        .height = FB_HEIGHT,
    });
}

static void frame(void) {

    // update framebuffer pixels with plasma effect
    state.time = fmodf(state.time + (float)sapp_frame_duration(), 3600.0f);
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            float time = state.time * 0.2f;
            float t3 = time * 3.0f;
            vec2_t coord = vec2((float)(x * 2), (float)(y * 2));
            float color1 = (sinf(vm_dot(coord, vec2(sinf(t3), cosf(t3))) * 0.02f + t3) + 1.0f) * 0.5f;
            vec2_t center = vm_add(vec2(320.0f, 180.0f), vec2(320.0f * sinf(-t3), 180.0 * cosf(-t3)));
            float color2 = (cosf(vm_length(vm_sub(coord, center)) * 0.03f) + 1.0f) * 0.5f;
            float color = color1 + color2;
            float rf = (cosf(M_PI * color + t3) + 1.0f) * 0.5f;
            float gf = (sinf(M_PI * color + t3) + 1.0f) * 0.5f;
            float bf = (sinf(t3) + 1.0f) * 0.5f;
            uint8_t ru8 = (uint8_t)(rf * 255.0f);
            uint8_t gu8 = (uint8_t)(gf * 255.0f);
            uint8_t bu8 = (uint8_t)(bf * 255.0f);
            pixels[y][x] = 0xFF000000 | (bu8<<16) | (gu8<<8) | ru8;
        }
    };

    // update framebuffer with plasma pixels outside a sokol-gfx pass
    sfb_update(state.fb, &(sfb_update_desc){ .pixels = SG_RANGE(pixels) });

    // draw framebuffer in sokol-gfx render pass
    sg_begin_pass(&(sg_pass){
        .action = { .colors[0] = { .load_action = SG_LOADACTION_DONTCARE } },
        .swapchain = sglue_swapchain(),
    });
    sfb_render(state.fb);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sfb_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .window_title = "framebuffer-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
