//------------------------------------------------------------------------------
//  debugtext-printf-sapp.c
//
//  Simple text rendering with sokol_debugtext.h, formatting, tabs, etc...
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#define SOKOL_DEBUGTEXT_ALL_FONTS
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"

typedef struct {
    uint8_t r, g, b;
} color_t;

static struct {
    sg_pass_action pass_action;
    uint32_t frame_count;
    uint64_t time_stamp;
    color_t palette[SDTX_NUM_FONTS];
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.125f, 0.25f, 1.0f } }
    },
    .palette = {
        { 0xf4, 0x43, 0x36 },
        { 0x21, 0x96, 0xf3 },
        { 0x4c, 0xaf, 0x50 },
        { 0xff, 0xeb, 0x3b },
        { 0x79, 0x86, 0xcb },
        { 0xff, 0x98, 0x00 },
    }
};


static void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());
    sdtx_setup(&(sdtx_desc_t){
        .context = {
            .color_format = sapp_color_format(),
            .depth_format = sapp_depth_format(),
            .sample_count = sapp_sample_count()
        }
    });
}

static void my_printf_wrapper(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    sdtx_vprintf(fmt, args);
    va_end(args);
}

static void frame(void) {
    state.frame_count++;
    double frame_time = stm_ms(stm_laptime(&state.time_stamp));

    sdtx_canvas(sapp_width() * 0.5f, sapp_height() * 0.5f);
    sdtx_origin(8.0f, 24.0f);
    for (int i = 0; i < SDTX_NUM_FONTS; i++) {
        color_t color = state.palette[i];
        sdtx_font(i);
        sdtx_color3b(color.r, color.g, color.b);
        sdtx_printf("Hello '%s'!\n", (state.frame_count & (1<<7)) ? "Welt" : "World");
        sdtx_printf("\tFrame Time:\t\t%.3f\n", frame_time);
        my_printf_wrapper("\tFrame Count:\t%d\t0x%04X\n", state.frame_count, state.frame_count);
        sdtx_crlf();
    }
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();

}

static void cleanup(void) {
    sdtx_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 640,
        .height = 480,
        .gl_force_gles2 = true,
        .window_title = "debugtext-printfsapp",
    };
}
