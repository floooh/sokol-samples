//------------------------------------------------------------------------------
//  spine-simple-sapp.c
//  An annotated 'simplest possible' sokol_spine.h sample.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "spine/spine.h"
#include "sokol_spine.h"
#include "sokol_glue.h"
#include "stb/stb_image.h"
#include "util/fileutil.h"
#include "dbgui/dbgui.h"

typedef struct {
    bool loaded;
    sspine_range data;
} load_status_t;

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instance;
    sg_pass_action pass_action;
    struct {
        load_status_t atlas;
        load_status_t skeleton;
        bool failed;
    } load_status;
    struct {
        uint8_t atlas[4 * 1024];
        uint8_t skeleton[128 * 1024];
        uint8_t image[512 * 1024];
    } buffers;
} state;

static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void create_spine_objects(void);

static void init(void) {
    // sokol-gfx must be setup before sokol-spine
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    // optional debugging UI, only active in the spine-simple-sapp-ui sample
    __dbgui_setup(sapp_sample_count());

    // Setup sokol_spine.h, if desired, memory usage can be tuned by
    // setting the max number of vertices, draw commands and pool sizes
    sspine_setup(&(sspine_desc){
        .max_vertices = 6 * 1024,
        .max_commands = 16,
        .atlas_pool_size = 1,
        .skeleton_pool_size = 1,
        .skinset_pool_size = 1,
        .instance_pool_size = 1,
        .logger = {
            .func = slog_func,
        },
    });

    // We'll use sokol_fetch.h for data loading because this gives us
    // asynchronous file loading which also works on the web.
    // The only downside is that spine initialization is spread
    // over a couple of callbacks and frames.
    // Configure sokol-fetch so that atlas and skeleton file
    // data are loaded in parallel across 2 channels.
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
        .logger.func = slog_func,
    });

    // Setup a sokol-gfx pass action to clear the default framebuffer to black
    // (used in sg_begin_pass() down in the frame callback)
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // Start loading the spine atlas and skeleton file. This happens asynchronously
    // and in undefined finish-order. The 'fetch callbacks' will be called when the data
    // has finished loading (or an error occurs).
    // sokol_spine.h itself doesn't care about how the data is loaded, it expects
    // all data in memory chunks.
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("raptor-pma.atlas", path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer = SFETCH_RANGE(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("raptor-pro.skel", path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer = SFETCH_RANGE(state.buffers.skeleton),
        .callback = skeleton_data_loaded,
    });
}

// sokol-fetch callback functions for loading the atlas and skeleton data.
// These are called in undefined order, but the spine atlas must be created
// before the skeleton (because the skeleton creation functions needs an
// atlas handle), this ordering problem is solved by both functions checking
// whether the other function has already finished, and if yes a common
// function 'create_spine_objects()' is called
static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        // atlas data was successfully loaded
        state.load_status.atlas = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ response->data.ptr, response->data.size },
        };
        // when both atlas and skeleton files have finished loading, create spine objects
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    } else if (response->failed) {
        // loading the atlas data failed
        state.load_status.failed = true;
    }
}

static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.skeleton = (load_status_t) {
            .loaded = true,
            .data = (sspine_range){ .ptr = response->data.ptr, .size = response->data.size }
        };
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    } else if (response->failed) {
        state.load_status.failed = true;
    }
}

// this function is called when both the spine atlas and skeleton file has been loaded,
// first an atlas object is created from the loaded atlas data, and then a skeleton
// object (which requires an atlas object as dependency), then a spine instance object.
// Finally any images required by the atlas object are loaded
static void create_spine_objects(void) {
    // Create spine atlas object from loaded atlas data.
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = state.load_status.atlas.data,
    });

    // Next create a spine skeleton object, skeleton data files can be either
    // text (JSON) or binary (in our case, 'raptor-pro.skel' is a binary skeleton file).
    // In case of JSON data, make sure that the data is 0-terminated!
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .binary_data = state.load_status.skeleton.data,
        // we can pre-scale the skeleton...
        .prescale = 0.5f,
        // and we can set the default animation mixing / cross-fading time
        .anim_default_mix = 0.2f,
    });

    // create a spine instance object, that's the thing that's actually rendered
    state.instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = state.skeleton,
    });

    // Since the spine instance doesn't move, its position can be set once,
    // the coordinate units depends on the sspine_layer_transform struct
    // that's passed to the sspine_draw_layer() during rendering (in our
    // case it's simply framebuffer pixels, with the origin in the
    // center)
    sspine_set_position(state.instance, (sspine_vec2){ .x = -100.0f, .y = 200.0f });

    // configure a simple animation sequence (jump => roar => walk)
    sspine_set_animation(state.instance, sspine_anim_by_name(state.skeleton, "jump"), 0, false);
    sspine_add_animation(state.instance, sspine_anim_by_name(state.skeleton, "roar"), 0, false, 0.0f);
    sspine_add_animation(state.instance, sspine_anim_by_name(state.skeleton, "walk"), 0, true, 0.0f);

    // Finally start loading any atlas image files, one image file seems to be
    // common, but apparently atlases can also reference multiple images.
    // Image loading also happens asynchronously via sokol-fetch, and the
    // actual sokol-gfx image creation happens in the fetch-callback.
    const int num_images = sspine_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image img = sspine_image_by_index(state.atlas, img_index);
        const sspine_image_info img_info = sspine_get_image_info(img);

        // We'll store the sspine_image handle in the fetch request's user data
        // blob, because we need the image info again later in the fetch callback
        // in order to initialize the sokol-gfx image with the right parameters.
        //
        // Also important to note: all image fetch requests load their data into the same
        // buffer. This is fine because sokol-fetch has been configured
        // with num_lanes=1, this will cause all requests on the same
        // channel to be serialized (not run in parallel). That way
        // the same buffer can be reused even if there are multiple atlas images.
        // The downside is that loading multiple images would take longer.
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .path = fileutil_get_path(img_info.filename.cstr, path_buf,  sizeof(path_buf)),
            .channel = 0,
            .buffer = SFETCH_RANGE(state.buffers.image),
            .callback = image_data_loaded,
            .user_data = SFETCH_RANGE(img)
        });
    }
}

// This is the image-data fetch callback. The loaded image data will be decoded
// via stb_image.h and a sokol-gfx image object will be created.
//
// What's interesting here is that we're using sokol-gfx's multi-step
// image setup. sokol-spine has already allocated an image handle
// for each atlas image in sspine_make_atlas() via sg_alloc_image().
//
// The fetch callback just needs to finish the image setup by
// calling sg_init_image(), or if loading has failed, put the
// image object into the 'failed' resource state.
//
static void image_data_loaded(const sfetch_response_t* response) {
    // retrieve the sspine_image handle from user data and look up image info
    // with image setup parameetrs
    const sspine_image img = *(sspine_image*)response->user_data;
    const sspine_image_info img_info = sspine_get_image_info(img);
    if (response->fetched) {
        // decode pixels via stb_image.h
        const int desired_channels = 4;
        int img_width, img_height, num_channels;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &img_width,
            &img_height,
            &num_channels, desired_channels);
        if (pixels) {
            // sokol-spine has already allocated an image, view and sampler handle,
            // just need to call sg_init_image(), sg_init_view() and sg_init_sampler() to complete setup
            sg_init_image(img_info.sgimage, &(sg_image_desc){
                .width = img_width,
                .height = img_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .label = img_info.filename.cstr,
                .data.mip_levels[0] = {
                    .ptr = pixels,
                    .size = (size_t)(img_width * img_height * 4)
                }
            });
            sg_init_view(img_info.sgview, &(sg_view_desc){
                .texture = { .image = img_info.sgimage },
            });
            sg_init_sampler(img_info.sgsampler, &(sg_sampler_desc){
                .min_filter = img_info.min_filter,
                .mag_filter = img_info.mag_filter,
                .mipmap_filter = img_info.mipmap_filter,
                .wrap_u = img_info.wrap_u,
                .wrap_v = img_info.wrap_v,
                .label = img_info.filename.cstr,
            });
            stbi_image_free(pixels);
        } else {
            // decoding has failed
            state.load_status.failed = true;
            // image decoding has failed, it's not strictly necessary, but
            // it's better here to put the sokol-gfx image object into
            // the 'failed' resource state (otherwise it would be stuck
            // in the 'alloc' state)
            sg_fail_image(img_info.sgimage);
        }
    } else {
        state.load_status.failed = true;
        sg_fail_image(img_info.sgimage);
    }
}

// frame callback, whoop whoop!
static void frame(void) {
    // need to call sfetch_dowork() once per frame, otherwise data loading will appear to be stuck
    sfetch_dowork();
    // the frame duration in seconds is needed for advancing the spine animations
    const float delta_time = (float)sapp_frame_duration();
    // use the window size for the spine canvas, this means that 'spine pixels'
    // will map 1:1 to framebuffer pixels, with [0,0] in the center
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const sspine_layer_transform layer_transform = {
        .size = { .x = w, .y = h },
        .origin = { .x = w * 0.5f, .y = h * 0.5f }
    };

    // Advance the instance animation and draw the instance.
    // Important to note here is that no actual sokol-gfx rendering happens yet,
    // instead sokol-spine will only record vertices, indices and draw commands.
    // Also, all sokol-spine functions can be called with invalid or 'incomplete'
    // handles, that way we don't need to care about whether the spine objects
    // have actually been created yet (because their data might still be loading)
    sspine_update_instance(state.instance, delta_time);
    sspine_draw_instance_in_layer(state.instance, 0);

    // The actual sokol-gfx render pass, here we also don't need to care about
    // if the atlas image have already been loaded yet, if the image handles
    // recorded by sokol-spine for rendering are not yet valid, rendering
    // operations will silently be skipped.
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sspine_draw_layer(0, &layer_transform);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sfetch_shutdown();
    sspine_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
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
        .window_title = "spine-simple-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
