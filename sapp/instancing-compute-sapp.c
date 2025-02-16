//------------------------------------------------------------------------------
//  instancing-compute-sapp.c
//
//  Same as instancing-sapp.c and instancing-pull-sapp.c, but compute the
//  particle positions in a compute shader.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "instancing-compute-sapp.glsl.h"

#define MAX_PARTICLES (512 * 1024)
#define MAX_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    uint64_t last_time;
    int num_particles;
    struct {
        sg_buffer buf;
        sg_pipeline pip;
    } compute;
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
} state = {
    .display = {
        .pass_action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.0f, 0.2f, 0.1f, 1.0f }
            }
        }
    }
};

static void draw_fallback(void);
static vs_params_t compute_vsparams(float frame_time);

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static void init(void) {

}

static void frame(void) {

}

static void cleanup(void) {

}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "instancing-compute-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
