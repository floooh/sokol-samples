//------------------------------------------------------------------------------
//  spine-layers-sapp.c
//  Use layered rendering to mix sokol-spine and sokol-gl rendering.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#define SOKOL_GL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
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

#define NUM_INSTANCES (3)

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
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sspine_setup(&(sspine_desc){
        .logger.func = slog_func
    });
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-gfx pass action to clear screen
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // start loading spine skeleton and atlas files in parallel
    char path[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("speedy-pma.atlas", path, sizeof(path)),
        .channel = 0,
        .buffer = SFETCH_RANGE(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("speedy-ess.skel", path, sizeof(path)),
        .channel = 1,
        .buffer = SFETCH_RANGE(state.buffers.skeleton),
        .callback = skeleton_data_loaded,
    });
}

static void frame(void) {
    const float delta_time = (float)sapp_frame_duration();
    sfetch_dowork();

    // use a fixed 'virtual' canvas size for the spine layer transform so that
    // the spine scene scales with the window size
    const sspine_layer_transform layer_transform = {
        .size = { .x = 1024.0f, .y = 768.0f },
        .origin = { .x = 512.0f, .y = 384.0f },
    };

    // draw 3 layers of vertical bars with sokol-gl, note how the order how we handle sokol-gl
    // vs sokol-spine rendering doesn't matter outside the sokol-gfx render pass
    sgl_defaults();
    for (int layer_index = 0; layer_index < 3; layer_index++) {
        sgl_layer(layer_index);
        switch (layer_index) {
            case 0: sgl_c3f(0.0f, 0.0f, 1.0f); break;
            case 1: sgl_c3f(0.0f, 1.0f, 0.0f); break;
            case 2: sgl_c3f(1.0f, 0.0f, 0.0f); break;
        }
        sgl_begin_quads();
        const int num_bars = 30;
        const float bar_width_half = 0.0075f;
        const float bar_offset_x = (float)layer_index * 0.02f;
        for (int i = 0; i <= num_bars; i++) {
            const float x = bar_offset_x + ((i * (1.0f / num_bars)) * 2.0f - 1.0f);
            const float x0 = x - bar_width_half;
            const float x1 = x + bar_width_half;
            sgl_v2f(x0, -1.0f); sgl_v2f(x1, -1.0f);
            sgl_v2f(x1, +1.0f); sgl_v2f(x0, +1.0f);
        }
        sgl_end();
    }

    // draw spine instances into different layers
    sspine_set_position(state.instances[0], (sspine_vec2){ -225.0f, 128.0f });
    sspine_update_instance(state.instances[0], delta_time);
    sspine_draw_instance_in_layer(state.instances[0], 0);

    sspine_set_position(state.instances[1], (sspine_vec2){ 0.0f, 128.0f });
    sspine_update_instance(state.instances[1], delta_time);
    sspine_draw_instance_in_layer(state.instances[1], 1);

    sspine_set_position(state.instances[2], (sspine_vec2){ +225.0f, 128.0f });
    sspine_update_instance(state.instances[2], delta_time);
    sspine_draw_instance_in_layer(state.instances[2], 2);

    // sokol-gfx render pass, draw the sokol-gl and sokol-spine layers interleaved
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    for (int layer_index = 0; layer_index < 3; layer_index++) {
        sspine_draw_layer(layer_index, &layer_transform);
        sgl_draw_layer(layer_index);
    }
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
            .data = (sspine_range){ .ptr = response->data.ptr, .size = response->data.size }
        };
        // when both atlas and skeleton data loaded, create spine objects
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    } else if (response->failed) {
        state.load_status.failed = true;
    }
}

// fetch callback for skeleton data
static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.skeleton = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ .ptr = response->data.ptr, .size = response->data.size }
        };
        // when both atlas and skeleton data loaded, create spine objects
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    } else if (response->failed) {
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
        sspine_set_animation(state.instances[i], sspine_anim_by_name(state.skeleton, "run-linear"), 0, true);
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
            .path = fileutil_get_path(img_info.filename.cstr, path_buf, sizeof(path_buf)),
            .buffer = SFETCH_RANGE(state.buffers.image),
            .callback = image_data_loaded,
            .user_data = SFETCH_RANGE(img),
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
            response->data.ptr,
            (int)response->data.size,
            &img_width,
            &img_height,
            &num_channels, desired_channels);
        if (pixels) {
            // sokol-spine has already allocated a sokol-gfx image, view and sampler handle for us,
            // now "populate" the handles with an actual image and sampler
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
            state.load_status.failed = true;
            sg_fail_image(img_info.sgimage);
        }
    } else if (response->failed) {
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
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
