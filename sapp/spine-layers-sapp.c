//------------------------------------------------------------------------------
//  spine-layers-sapp.c
//  Demonstrates sokol_spine.h layered rendering mixed with sokol_gl.h
//  rendering.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#define SOKOL_GL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "spine/spine.h"
#include "sokol_spine.h"
#include "sokol_gl.h"
#include "sokol_glue.h"
#include "stb/stb_image.h"
#include "util/fileutil.h"
#include "dbgui/dbgui.h"

typedef struct {
    bool loaded;
    sspine_range data;
} load_status_t;

#define NUM_INSTANCES (2)

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instances[NUM_INSTANCES];
    sg_pass_action pass_action;
    struct {
        load_status_t atlas;
        load_status_t skeleton;
        bool failed;
    } load_status;
    struct {
        uint8_t atlas[16 * 1024];
        uint8_t skeleton[512 * 1024];
        uint8_t image[512 * 1024];
    } buffers;
} state;

static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void create_spine_objects(void);

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    sspine_setup(&(sspine_desc){0});
    sgl_setup(&(sgl_desc_t){0});
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
    });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-gfx pass action to clear screen
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.625f, 0.08f, 0.33f, 1.0f } }
    };

    // start loading spine skeleton and atlas files in parallel
    char path[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("speedy-pma.atlas", path, sizeof(path)),
        .channel = 0,
        .buffer_ptr = state.buffers.atlas,
        .buffer_size = sizeof(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("speedy-ess.skel", path, sizeof(path)),
        .channel = 1,
        .buffer_ptr = state.buffers.skeleton,
        .buffer_size = sizeof(state.buffers.skeleton),
        .callback = skeleton_data_loaded,
    });
}

static void frame(void) {
    const double delta_time = sapp_frame_duration();
    sfetch_dowork();
    sspine_new_frame();

    // use a fixed 'virtual' canvas size for the spine layer transform so that
    // the spine scene scales with the window size
    const sspine_layer_transform layer_transform = {
        .size = { .x = 1024.0f, .y = 768.0f },
        .origin = { .x = 512.0f, .y = 384.0f },
    };

    // draw a couple of bars with sokol-gl, note how the order how we handle sokol-gl
    // and sokol-spine rendering doesn't matter outside the sokol-gfx render pass
    sgl_defaults();
    sgl_c3f(0.0f, 0.75f, 0.0f);
    sgl_begin_quads();
    const int num_bars = 30;
    const float bar_width_half = 0.02f;
    for (int i = 0; i <= num_bars; i++) {
        const float x = (i * (1.0f / num_bars)) * 2.0f - 1.0f;
        const float x0 = x - bar_width_half;
        const float x1 = x + bar_width_half;
        sgl_v2f(x0, -1.0f); sgl_v2f(x1, -1.0f);
        sgl_v2f(x1, +1.0f); sgl_v2f(x0, +1.0f);
    }
    sgl_end();

    // draw first spine instance into layer 0
    sspine_set_position(state.instances[0], (sspine_vec2){ -225.0f, 128.0f });
    sspine_update_instance(state.instances[0], delta_time);
    sspine_draw_instance_in_layer(state.instances[0], 0);

    // draw second spine instance into layer 1
    sspine_set_position(state.instances[1], (sspine_vec2){ +225.0f, 128.0f });
    sspine_update_instance(state.instances[1], delta_time);
    sspine_draw_instance_in_layer(state.instances[1], 1);

    // sokol-gfx render pass
    // - first draw the sspine layer 0
    // - then draw the sokol-gl scene
    // - then draw the sspine layer 1
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sspine_draw_layer(0, &layer_transform);
    sgl_draw();
    sspine_draw_layer(1, &layer_transform);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sfetch_shutdown();
    sgl_shutdown();
    sspine_shutdown();
    sg_shutdown();
}

// fetch callback for atlas data
static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.atlas = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ .ptr = response->buffer_ptr, .size = response->fetched_size }
        };
        // when both atlas and skeleton data loaded, create spine objects
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// fetch callback for skeleton data
static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.skeleton = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ .ptr = response->buffer_ptr, .size = response->fetched_size }
        };
        // when both atlas and skeleton data loaded, create spine objects
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// called when both the atlas and skeleton files have been loaded,
// creates spine atlas, skeleton and instance, and starts loading
// the atlas image(s)
static void create_spine_objects(void) {
    // create spine atlas object
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = state.load_status.atlas.data
    });
    assert(sspine_atlas_valid(state.atlas));

    // create spine skeleton object
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .anim_default_mix = 0.2f,
        .binary_data = state.load_status.skeleton.data,
    });
    assert(sspine_skeleton_valid(state.skeleton));

    // create two instance objects
    for (int i = 0; i < NUM_INSTANCES; i++) {
        state.instances[i] = sspine_make_instance(&(sspine_instance_desc){
            .skeleton = state.skeleton
        });
        assert(sspine_instance_valid(state.instances[i]));
        sspine_set_animation(state.instances[i], sspine_anim_by_name(state.skeleton, (i & 1) ? "run-linear" : "run"), 0, true);
    }

    // start loading atlas images
    const int num_images = sspine_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image img = sspine_image_by_index(state.atlas, img_index);
        const sspine_image_info img_info = sspine_get_image_info(img);
        assert(img_info.valid);
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .channel = 0,
            .path = fileutil_get_path(img_info.filename, path_buf, sizeof(path_buf)),
            .buffer_ptr = state.buffers.image,
            .buffer_size = sizeof(state.buffers.image),
            .callback = image_data_loaded,
            .user_data_ptr = &img,
            .user_data_size = sizeof(img),
        });
    }
}

// load spine atlas image data and create a sokol-gfx image object
static void image_data_loaded(const sfetch_response_t* response) {
    const sspine_image img = *(sspine_image*)response->user_data;
    const sspine_image_info img_info = sspine_get_image_info(img);
    assert(img_info.valid);
    if (response->fetched) {
        // decode pixels via stb_image.h
        const int desired_channels = 4;
        int img_width, img_height, num_channels;
        stbi_uc* pixels = stbi_load_from_memory(
            response->buffer_ptr,
            (int)response->fetched_size,
            &img_width,
            &img_height,
            &num_channels, desired_channels);
        if (pixels) {
            // sokol-spine has already allocated a sokol-gfx image handle for use,
            // now "populate" the handle with an actual image
            sg_init_image(img_info.sgimage, &(sg_image_desc){
                .width = img_width,
                .height = img_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = img_info.min_filter,
                .mag_filter = img_info.mag_filter,
                .label = img_info.filename,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(img_width * img_height * 4)
                }
            });
            stbi_image_free(pixels);
        }
        else {
            state.load_status.failed = true;
            sg_fail_image(img_info.sgimage);
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
        sg_fail_image(img_info.sgimage);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 1024,
        .height = 768,
        .window_title = "spine-layers-sapp.c",
        .icon.sokol_default = true
    };
}
