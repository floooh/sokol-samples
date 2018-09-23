//------------------------------------------------------------------------------
//  modplay-sapp.c
//  sokol_app + sokol_audio + libmodplug
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_audio.h"
#include "modplug.h"
#include "data/mods.h"
#include <assert.h>

static bool mpf_valid = false;
static ModPlugFile* mpf;

/* integer-to-float conversion buffer */
#define SRCBUF_SAMPLES (4096)
static int src_buf[SRCBUF_SAMPLES];

static void stream_cb(float* buffer, int num_samples) {
    assert(num_samples < SRCBUF_SAMPLES);
    if (mpf_valid) {
        /* read sampled from libmodplug, and convert to float */
        int res = ModPlug_Read(mpf, (void*)src_buf, sizeof(int)*num_samples);
        int samples_in_buffer = res / sizeof(int);
        int i;
        for (i = 0; i < samples_in_buffer; i++) {
            buffer[i] = src_buf[i] / (float)0x7fffffff;
        }
        for (; i < num_samples; i++) {
            buffer[i] = 0.0f;
        }
    }
    else {
        /* if file wasn't loaded, fill the output buffer with silence */
        for (int i = 0; i < num_samples; i++) {
            buffer[i] = 0.0f;
        }
    }
}

void init(void) {
    /* setup sokol_gfx */
    sg_setup(&(sg_desc){
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });

    /* setup sokol_audio (default is 44100hz, mono) */
    saudio_setup(&(saudio_desc){
        .stream_cb = stream_cb
    });

    /* setup libmodplug and load mod from embedded C array */
    ModPlug_Settings mps;
    ModPlug_GetSettings(&mps);
    mps.mChannels = 1;
    mps.mBits = 32;
    mps.mFrequency = saudio_sample_rate();
    mps.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
    mps.mMaxMixChannels = 64;
    mps.mLoopCount = -1;
    mps.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
    ModPlug_SetSettings(&mps);

    mpf = ModPlug_Load(dump_comsi, sizeof(dump_comsi));
    if (mpf) {
        mpf_valid = true;
    }
}

void frame(void) {
    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.5f, 1.0f, 0.0f, 1.0f } }
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    saudio_shutdown();
    if (mpf_valid) {
        ModPlug_Unload(mpf);
    }
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 400,
        .height = 300,
        .window_title = "Sokol Audio + LibModPlug",
    };
}
