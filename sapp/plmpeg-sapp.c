//------------------------------------------------------------------------------
//  plmpeg-sapp.c
//  https://github.com/phoboslab/pl_mpeg
//  FIXME: needs proper file streaming
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "dbgui/dbgui.h"
#include "plmpeg-sapp.glsl.h"
#include "pl_mpeg/pl_mpeg.h"
#include <assert.h>
#include <stdlib.h>

// FIXME!
static const char* filename = "/Users/floh/scratch/bjork-all-is-full-of-love.mpg";

static struct {
    plm_t* plm;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image_desc img_desc_y;
    sg_image_desc img_desc_cb;
    sg_image_desc img_desc_cr;
} state;

static void video_cb(plm_t *mpeg, plm_frame_t *frame, void *user);
static void audio_cb(plm_t *mpeg, plm_samples_t *samples, void *user);

void init(void) {
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

    // initialize plmpeg, load the video file, install decode callbacks
    // FIXME: this needs to be changed to streaming via the load callback
    state.plm = plm_create_with_filename(filename);
    assert(state.plm);
    plm_set_video_decode_callback(state.plm, video_cb, 0);
    plm_set_audio_decode_callback(state.plm, audio_cb, 0);
    plm_set_loop(state.plm, true);
    plm_set_audio_enabled(state.plm, true, 0);
    if (plm_get_num_audio_streams(state.plm) > 0) {
        // FIXME: setup sokol-audio
    }

    // a vertex buffer to render a 'fullscreen triangle'
    float fsq_verts[] = { -1.0f, -3.0f, 3.0f, 1.0f, -1.0f, 1.0f };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(fsq_verts),
        .content = fsq_verts,
        .label = "plmpeg vertices"
    });

    // shader and pipeline object for rendering a fullscreen quad
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

    // NOTE: image creation is deferred until first frame is decoded
}

void frame(void) {
    plm_decode(state.plm, 1.0/60.0);
    int w = sapp_width();
    int h = sapp_height();
    sg_begin_default_pass(&state.pass_action, w, h);
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    plm_destroy(state.plm);
    sg_shutdown();
}

// create or re-create a texture image where decoded plane data goes
sg_image validate_image(sg_image_desc* desc, sg_image old_image, int w, int h) {
    if ((desc->width != w) || (desc->height != h)) {
        // it's ok to call sg_destroy_image with SG_INVALID_ID
        sg_destroy_image(old_image);
        *desc = (sg_image_desc) {
            .width = w,
            .height = h,
            .pixel_format = SG_PIXELFORMAT_L8,
            .usage = SG_USAGE_STREAM,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        };
        return sg_make_image(desc);
    }
    else {
        return old_image;
    }
}

void video_cb(plm_t* mpeg, plm_frame_t* frame, void* user) {
    // (re-)create images on demand
    state.bind.fs_images[SLOT_tex_y] = validate_image(
        &state.img_desc_y,
        state.bind.fs_images[SLOT_tex_y],
        frame->y.width,
        frame->y.height);

    state.bind.fs_images[SLOT_tex_cb] = validate_image(
        &state.img_desc_cb,
        state.bind.fs_images[SLOT_tex_cb],
        frame->cb.width,
        frame->cb.height);

    state.bind.fs_images[SLOT_tex_cr] = validate_image(
        &state.img_desc_cr,
        state.bind.fs_images[SLOT_tex_cr],
        frame->cr.width,
        frame->cr.height);

    // copy over image plane data
    sg_update_image(state.bind.fs_images[SLOT_tex_y], &(sg_image_content){
        .subimage[0][0] = {
            .ptr = frame->y.data,
            .size = frame->y.width * frame->y.height
        }
    });
    sg_update_image(state.bind.fs_images[SLOT_tex_cb], &(sg_image_content){
        .subimage[0][0] = {
            .ptr = frame->cb.data,
            .size = frame->cb.width * frame->cb.height
        }
    });
    sg_update_image(state.bind.fs_images[SLOT_tex_cr], &(sg_image_content){
        .subimage[0][0] = {
            .ptr = frame->cr.data,
            .size = frame->cr.width * frame->cr.height
        }
    });
}

void audio_cb(plm_t* mpeg, plm_samples_t* samples, void* user) {
    // FIXME!
}

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
