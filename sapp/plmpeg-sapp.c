//------------------------------------------------------------------------------
//  plmpeg-sapp.c
//  https://github.com/phoboslab/pl_mpeg
//  FIXME: needs proper file streaming
//------------------------------------------------------------------------------
#include <assert.h>
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_audio.h"
#include "dbgui/dbgui.h"
#include "pl_mpeg/pl_mpeg.h"
#include "plmpeg-sapp.glsl.h"

// FIXME!
static const char* filename = "/Users/floh/scratch/bjork-all-is-full-of-love.mpg";

static struct {
    plm_t* plm;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    struct {
        int width;
        int height;
    } image_attrs[3];
} state;

static void video_cb(plm_t *mpeg, plm_frame_t *frame, void *user);
static void audio_cb(plm_t *mpeg, plm_samples_t *samples, void *user);

// the sokol-app init-callback
static void init(void) {
    // initialize plmpeg, load video file
    // FIXME: this needs to be changed to streaming via the load callback
    state.plm = plm_create_with_filename(filename);
    assert(state.plm);
    plm_set_video_decode_callback(state.plm, video_cb, 0);
    plm_set_audio_decode_callback(state.plm, audio_cb, 0);
    plm_set_loop(state.plm, true);
    plm_set_audio_enabled(state.plm, true, 0);
    if (plm_get_num_audio_streams(state.plm) > 0) {
        saudio_setup(&(saudio_desc){
            .sample_rate = plm_get_samplerate(state.plm),
            .num_channels = 2,
        });
    }

    // initialize sokol-gfx
    sg_setup(&(sg_desc){
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });
    __dbgui_setup(1);

    // create a vertex buffer and pipeline object to render a 'fullscreen triangle'
    const float fsq_verts[] = { -1.0f, -3.0f, 3.0f, 1.0f, -1.0f, 1.0f };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(fsq_verts),
        .content = fsq_verts,
    });
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT2
        },
        .shader = sg_make_shader(plmpeg_shader_desc()),
    });

    // don't need to clear since the whole framebuffer is overwritten
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_DONTCARE }
    };

    // NOTE: texture creation is deferred until first frame is decoded
}

// the sokol-app frame callback (video decoding and rendering)
static void frame(void) {
    plm_decode(state.plm, 1.0/60.0);
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

// the sokol-sapp cleanup callback
static void cleanup(void) {
    __dbgui_shutdown();
    plm_destroy(state.plm);
    sg_shutdown();
}

// (re-)create a plane texture on demand, and update it with decoded video-plane data
static void validate_texture(int slot, plm_plane_t* plane) {

    if ((state.image_attrs[slot].width != (int)plane->width) ||
        (state.image_attrs[slot].height != (int)plane->height))
    {
        // NOTE: it's ok call to call sg_destroy_image() with SG_INVALID_ID
        sg_destroy_image(state.bind.fs_images[slot]);
        state.bind.fs_images[slot] = sg_make_image(&(sg_image_desc){
            .width = plane->width,
            .height = plane->height,
            .pixel_format = SG_PIXELFORMAT_L8,
            .usage = SG_USAGE_STREAM,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        });
    }

    // copy decoded plane pixels into texture
    sg_update_image(state.bind.fs_images[slot], &(sg_image_content){
        .subimage[0][0] = {
            .ptr = plane->data,
            .size = plane->width * plane->height * sizeof(uint8_t)
        }
    });
}

// the pl_mpeg video callback, copies decoded video data into textures
static void video_cb(plm_t* mpeg, plm_frame_t* frame, void* user) {
    validate_texture(SLOT_tex_y, &frame->y);
    validate_texture(SLOT_tex_cb, &frame->cb);
    validate_texture(SLOT_tex_cr, &frame->cr);
}

// the pl_mpeg audio callback, forwards decoded audio samples to sokol-audio
static void audio_cb(plm_t* mpeg, plm_samples_t* samples, void* user) {
    saudio_push(samples->interleaved, samples->count);
}

// sokol-app entry function
sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 960,
        .height = 540,
        .gl_force_gles2 = true,
        .window_title = "pl_mpeg demo"
    };
}
