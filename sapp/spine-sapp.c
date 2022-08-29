//------------------------------------------------------------------------------
//  spine-sapp.c
//  Simple sokol_spine.h sample
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#include "util/fileutil.h"
#define SOKOL_SPINE_IMPL
#include "spine/spine.h"
#include "sokol_spine.h"
#include "stb/stb_image.h"
#include "dbgui/dbgui.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instance;
    sg_pass_action pass_action;
    struct {
        bool failed;
        int load_count;
        size_t atlas_data_size;
    } load_status;
    struct {
        uint8_t atlas[8 * 1024];
        uint8_t skeleton[256 * 1024];
        uint8_t image[256 * 1024];
    } buffers;
} state;

static void setup_spine_objects(void);
static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-fetch for loading up to 2 files in parallel
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
    });

    // setup sokol-spine with default attributes
    sspine_setup(&(sspine_desc){0});

    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.5f, 0.7f, 1.0f} }
    };

    // start loading Spine atlas and skeleton file asynchronously
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("spineboy.atlas", path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer_ptr = state.buffers.atlas,
        .buffer_size = sizeof(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("spineboy-pro.json", path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer_ptr = state.buffers.skeleton,
        // the skeleton file is text data, make sure we have room for a terminating zero
        .buffer_size = sizeof(state.buffers.skeleton) - 1,
        .callback = skeleton_data_loaded,
    });
}

static void frame(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const double delta_time = sapp_frame_duration();

    sfetch_dowork();

    // can call Spine drawing functions with invalid or 'incomplete' object handles
    sspine_new_frame();
    sspine_update_instance(state.instance, delta_time);
    sspine_draw_instance_in_layer(state.instance, 0);

    // the actual sokol-gfx render pass
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    // NOTE: using the display width/height here means the Spine rendering
    // is mapped to pixels and doesn't scale with window size
    sspine_draw_layer(0, &(sspine_layer_transform){
        .size   = { .x = w, .y = h },
        .origin = { .x = w * 0.5f, .y = h * 0.8f }
    });
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    __dbgui_event(ev);
}

static void cleanup(void) {
    __dbgui_shutdown();
    sspine_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

// called by sokol-fetch when atlas file has been loaded
static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.atlas_data_size = response->fetched_size;
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (++state.load_status.load_count == 2) {
            setup_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        // the loaded data file is JSON text, make sure it's zero terminated
        assert(response->fetched_size < sizeof(state.buffers.skeleton));
        state.buffers.skeleton[response->fetched_size] = 0;
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (++state.load_status.load_count == 2) {
            setup_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// called when both the Spine atlas and skeleton file has finished loading
static void setup_spine_objects(void) {
    // create atlas from file data
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = {
            .ptr = state.buffers.atlas,
            .size = state.load_status.atlas_data_size,
        }
    });
    assert(sspine_atlas_valid(state.atlas));

    // create a skeleton object
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .prescale = 0.75f,
        .anim_default_mix = 0.2f,
        // we already made sure the JSON text data is zero-terminated
        .json_data = (const char*)&state.buffers.skeleton,
    });
    assert(sspine_skeleton_valid(state.skeleton));

    // create a skeleton instance
    state.instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = state.skeleton,
    });
    assert(sspine_instance_valid(state.instance));
    sspine_set_animation_by_name(state.instance, 0, "portal", false);
    sspine_add_animation_by_name(state.instance, 0, "run", true, 0.0f);

    // asynchronously load atlas images
    const int num_images = sspine_get_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image_info img_info = sspine_get_image_info(state.atlas, img_index);
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .channel = 0,
            .path = fileutil_get_path(img_info.filename, path_buf, sizeof(path_buf)),
            .buffer_ptr = state.buffers.image,
            .buffer_size = sizeof(state.buffers.image),
            .callback = image_data_loaded,
            // sspine_image_info is small enough to directly fit into the user data payload
            .user_data_ptr = &img_info,
            .user_data_size = sizeof(img_info)
        });
    }
}

// called when atlas image data has finished loading
static void image_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        const sspine_image_info* img_info = (sspine_image_info*)response->user_data;
        // decode image via stb_image.h
        const int desired_channels = 4;
        int img_width, img_height, num_channels;
        stbi_uc* pixels = stbi_load_from_memory(
            response->buffer_ptr,
            (int)response->fetched_size,
            &img_width, &img_height,
            &num_channels, desired_channels);
        if (pixels) {
            // sokol-spine has already allocated a sokol-gfx image handle for use,
            // now "populate" the handle with an actual image
            sg_init_image(img_info->image, &(sg_image_desc){
                .width = img_width,
                .height = img_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = img_info->min_filter,
                .mag_filter = img_info->mag_filter,
                .label = img_info->filename,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(img_width * img_height * 4)
                }
            });
            stbi_image_free(pixels);
        }
        else {
            state.load_status.failed = false;
            sg_fail_image(img_info->image);
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "spine-sapp.c",
        .icon.sokol_default = true,
    };
}
