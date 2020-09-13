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
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"

#define NUM_FONTS  (3)
#define FONT_KC854 (0)
#define FONT_C64   (1)
#define FONT_ORIC  (2)

typedef struct {
    uint8_t r, g, b;
} color_t;

static struct {
    sg_pass_action pass_action;
    uint32_t frame_count;
    uint64_t time_stamp;
    color_t palette[NUM_FONTS];
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.125f, 0.25f, 1.0f } }
    },
    .palette = {
        { 0xf4, 0x43, 0x36 },
        { 0x21, 0x96, 0xf3 },
        { 0x4c, 0xaf, 0x50 },
    }
};


static void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());
    sdtx_setup(&(sdtx_desc_t){
        .fonts = {
            [FONT_KC854] = sdtx_font_kc854(),
            [FONT_C64]   = sdtx_font_c64(),
            [FONT_ORIC]  = sdtx_font_oric()
        },
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
    sdtx_origin(3.0f, 3.0f);
    for (int i = 0; i < NUM_FONTS; i++) {
        color_t color = state.palette[i];
        sdtx_font(i);
        sdtx_color3b(color.r, color.g, color.b);
        sdtx_printf("Hello '%s'!\n", (state.frame_count & (1<<7)) ? "Welt" : "World");
        sdtx_printf("\tFrame Time:\t\t%.3f\n", frame_time);
        my_printf_wrapper("\tFrame Count:\t%d\t0x%04X\n", state.frame_count, state.frame_count);
        sdtx_putr("Range Test 1(xyzbla)", 12);
        sdtx_putr("\nRange Test 2\n", 32);
        sdtx_move_y(2);
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
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 640,
        .height = 480,
        .gl_force_gles2 = true,
        .window_title = "debugtext-printf-sapp",
    };
}
