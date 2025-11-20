//------------------------------------------------------------------------------
//  spine-switch-skinsets-sapp.c
//  Test if switching skinsets on the same instance works.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_fetch.h"
#include "sokol_debugtext.h"
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

#define NUM_SKINSETS (3)
#define NUM_SKINS_PER_SKINSET (8)

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instance;
    sspine_skinset skinsets[NUM_SKINSETS];
    sg_pass_action pass_action;
    struct {
        load_status_t atlas;
        load_status_t skeleton;
    } load_status;
    struct {
        uint8_t atlas[16 * 1024];
        uint8_t skeleton[300 * 1024];
        uint8_t image[512 * 1024];
    } buffers;
} state;

static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void create_spine_objects(void);

static const char* skins[NUM_SKINSETS][NUM_SKINS_PER_SKINSET] = {
    {
        "skin-base",
        "accessories/backpack",
        "clothes/dress-blue",
        "eyelids/girly",
        "eyes/eyes-blue",
        "hair/blue",
        "legs/boots-pink",
        "nose/long",
    },
    {
        "skin-base",
        "accessories/bag",
        "clothes/dress-green",
        "eyelids/semiclosed",
        "eyes/green",
        "hair/brown",
        "legs/boots-red",
        "nose/short",
    },
    {
        "skin-base",
        "accessories/cape-blue",
        "clothes/hoodie-blue-and-scarf",
        "eyelids/girly",
        "eyes/violet",
        "hair/pink",
        "legs/pants-green",
        "nose/long",
    }
};

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_kc854(),
        .logger.func = slog_func,
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
        .logger.func = slog_func,
    });
    sspine_setup(&(sspine_desc){ .logger.func = slog_func });
    __dbgui_setup(sapp_sample_count());

    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("mix-and-match-pma.atlas", path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer = SFETCH_RANGE(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("mix-and-match-pro.skel", path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer = SFETCH_RANGE(state.buffers.skeleton),
        .callback = skeleton_data_loaded,
    });
}

static void frame(void) {
    sfetch_dowork();
    const float delta_time = (float) sapp_frame_duration();
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const sspine_layer_transform layer_transform = {
        .size = { .x = w, .y = h },
        .origin = { .x = w * 0.5f, .y = h * 0.5f }
    };

    sdtx_canvas(w * 0.5f, h * 0.5f);
    sdtx_origin(2.0f, 2.0f);
    sdtx_home();
    sdtx_puts("Press 1, 2, 3 to switch skin sets!");

    sspine_update_instance(state.instance, delta_time);
    sspine_draw_instance_in_layer(state.instance, 0);

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sspine_draw_layer(0, &layer_transform);
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sspine_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        switch (ev->key_code) {
            case SAPP_KEYCODE_1:
                sspine_set_skinset(state.instance, state.skinsets[0]);
                break;
            case SAPP_KEYCODE_2:
                sspine_set_skinset(state.instance, state.skinsets[1]);
                break;
            case SAPP_KEYCODE_3:
                sspine_set_skinset(state.instance, state.skinsets[2]);
                break;
            default:
                break;
        }
    }
    __dbgui_event(ev);
}

static void load_failed(void) {
    state.pass_action.colors[0].clear_value = (sg_color){ 1.0f, 0.0f, 0.0f, 1.0f };
}

static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.atlas = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ .ptr = response->data.ptr, .size = response->data.size }
        };
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    } else if (response->failed) {
        load_failed();
    }
}

static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.skeleton = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ .ptr = response->data.ptr, .size = response->data.size }
        };
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    } else if (response->failed) {
        load_failed();
    }
}

static void create_spine_objects(void) {
    // create atlas, skeleton and instance object
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = state.load_status.atlas.data
    });
    assert(sspine_atlas_valid(state.atlas));

    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .binary_data = state.load_status.skeleton.data
    });

    state.instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = state.skeleton
    });
    sspine_set_scale(state.instance, (sspine_vec2){ 0.75f, 0.75f });
    sspine_set_position(state.instance, (sspine_vec2){ 0.0f, 220.0f });
    sspine_set_animation(state.instance, sspine_anim_by_name(state.skeleton, "walk"), 0, true);

    // start loading atlas images
    const int num_images = sspine_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image img = sspine_image_by_index(state.atlas, img_index);
        const sspine_image_info img_info = sspine_get_image_info(img);
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .path = fileutil_get_path(img_info.filename.cstr, path_buf, sizeof(path_buf)),
            .channel = 0,
            .buffer = SFETCH_RANGE(state.buffers.image),
            .callback = image_data_loaded,
            .user_data = SFETCH_RANGE(img),
        });
    }

    // create skinsets
    for (int skinset_index = 0; skinset_index < NUM_SKINSETS; skinset_index++) {
        sspine_skinset_desc desc = {
            .skeleton = state.skeleton
        };
        for (int skin_index = 0; skin_index < NUM_SKINS_PER_SKINSET; skin_index++) {
            desc.skins[skin_index] = sspine_skin_by_name(state.skeleton, skins[skinset_index][skin_index]);
        }
        state.skinsets[skinset_index] = sspine_make_skinset(&desc);
    }

    // set the first skinset as visible
    sspine_set_skinset(state.instance, state.skinsets[0]);
}

static void image_data_loaded(const sfetch_response_t* response) {
    const sspine_image img = *(sspine_image*)response->user_data;
    const sspine_image_info img_info = sspine_get_image_info(img);
    assert(img_info.valid);
    if (response->fetched) {
        const int desired_channels = 4;
        int img_width, img_height, num_channels;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &img_width,
            &img_height,
            &num_channels, desired_channels);
        if (pixels) {
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
            sg_init_view(img_info.sgview, &(sg_view_desc){ .texture.image = img_info.sgimage });
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
            sg_fail_image(img_info.sgimage);
            load_failed();
        }
    } else if (response->failed) {
        sg_fail_image(img_info.sgimage);
        load_failed();
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
        .window_title = "spine-switch-skinsets-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
