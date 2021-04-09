//------------------------------------------------------------------------------
//  saudio-sapp.c
//  Test sokol-audio
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_audio.h"
#include "sokol_glue.h"

#define NUM_SAMPLES (32)

static struct {
    sg_pass_action pass_action;
    uint32_t even_odd;
    int sample_pos;
    float samples[NUM_SAMPLES];
} state;

void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    saudio_setup(&(saudio_desc){0});
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.5f, 0.0f, 1.0f } }
    };
}

void frame(void) {
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    int num_frames = saudio_expect();
    float s;
    for (int i = 0; i < num_frames; i++) {
        if (state.even_odd++ & (1<<5)) {
            s = 0.05f;
        }
        else {
            s = -0.05f;
        }
        state.samples[state.sample_pos++] = s;
        if (state.sample_pos == NUM_SAMPLES) {
            state.sample_pos = 0;
            saudio_push(state.samples, NUM_SAMPLES);
        }
    }
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    saudio_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 400,
        .height = 300,
        .gl_force_gles2 = true,
        .window_title = "Sokol Audio Test",
        .icon.sokol_default = true,
    };
}

