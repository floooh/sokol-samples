//------------------------------------------------------------------------------
//  spine-skinsets-sapp.c
//  Test/demonstrate skinset usage and draw call merging.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_debugtext.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "spine/spine.h"
#include "sokol_spine.h"
#include "stb/stb_image.h"
#include "util/fileutil.h"
#include "dbgui/dbgui.h"

#define NUM_INSTANCES_X (20)
#define NUM_INSTANCES_Y (7)
#define NUM_INSTANCES (NUM_INSTANCES_X * NUM_INSTANCES_Y)
#define NUM_SKINS (8)

typedef struct {
    bool loaded;
    sspine_range data;
} load_status_t;

typedef struct {
    sspine_vec2 pos;
    sspine_vec2 vec;
} cell_t;

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instances[NUM_INSTANCES];
    sg_pass_action pass_action;
    cell_t grid[NUM_INSTANCES];
    struct {        
        load_status_t atlas;
        load_status_t skeleton;
        bool failed;
    } load_status;
    struct {
        uint8_t atlas[8 * 1024];
        uint8_t skeleton[300 * 1024];
        uint8_t image[512 * 1024];
    } buffers;
} state;

// unique skins to be combined into skin sets
const char* accessories[NUM_SKINS] = {
    "accessories/backpack",
    "accessories/bag",
    "accessories/cape-blue",
    "accessories/cape-red",
    "accessories/hat-pointy-blue-yellow",
    "accessories/hat-red-yellow",
    "accessories/scarf",
    "accessories/backpack",
};
const char* clothes[NUM_SKINS] = {
    "clothes/dress-blue",
    "clothes/dress-green",
    "clothes/hoodie-blue-and-scarf",
    "clothes/hoodie-orange",
    "clothes/dress-blue",
    "clothes/dress-green",
    "clothes/hoodie-blue-and-scarf",
    "clothes/hoodie-orange"
};
const char* eyelids[NUM_SKINS] = {
    "eyelids/girly",
    "eyelids/semiclosed",
    "eyelids/girly",
    "eyelids/semiclosed",
    "eyelids/girly",
    "eyelids/semiclosed",
    "eyelids/girly",
    "eyelids/semiclosed",
};
const char* eyes[NUM_SKINS] = {
    "eyes/eyes-blue",
    "eyes/green",
    "eyes/violet",
    "eyes/yellow",
    "eyes/eyes-blue",
    "eyes/green",
    "eyes/violet",
    "eyes/yellow",
};
const char* hair[NUM_SKINS] = {
    "hair/blue",
    "hair/brown",
    "hair/long-blue-with-scarf",
    "hair/pink",
    "hair/short-red",
    "hair/blue",
    "hair/brown",
    "hair/long-blue-with-scarf",
};
const char* legs[NUM_SKINS] = {
    "legs/boots-pink",
    "legs/boots-red",
    "legs/pants-green",
    "legs/pants-jeans",
    "legs/boots-pink",
    "legs/boots-red",
    "legs/pants-green",
    "legs/pants-jeans"
};
const char* nose[NUM_SKINS] = {
    "nose/long",
    "nose/short",
    "nose/long",
    "nose/short",
    "nose/long",
    "nose/short",
    "nose/long",
    "nose/short",
};

static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void create_spine_objects(void);

static void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric()
    });
    sspine_setup(&(sspine_desc){
        .skinset_pool_size = NUM_INSTANCES,
        .instance_pool_size = NUM_INSTANCES,
        .max_vertices = 256 * 1024,
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
    });

    // pass action to clear to blue-ish
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };

    // start loading spine atlas file
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("mix-and-match-pma.atlas", path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer_ptr = state.buffers.atlas,
        .buffer_size = sizeof(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });

    // start loading spine skeleton file in parallel
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("mix-and-match-pro.skel", path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer_ptr = state.buffers.skeleton,
        .buffer_size = sizeof(state.buffers.skeleton),
        .callback = skeleton_data_loaded,
    });

    // initialize the entity position grid
    const float dx = 64.0f;
    const float dy = 96.0f;
    float y = -dy * (NUM_INSTANCES_Y/2) + dy * 0.5f;
    for (int iy = 0; iy < NUM_INSTANCES_Y; iy++) {
        float x = -dx * (NUM_INSTANCES_X/2) + dx * 0.5f;
        for (int ix = 0; ix < NUM_INSTANCES_X; ix++) {
            state.grid[iy * NUM_INSTANCES_X + ix].pos = (sspine_vec2){ x, y };
            x += dx;
        }
        y += dy;
    }
}

static void frame(void) {
    const double delta_time = sapp_frame_duration();
    const float width = sapp_widthf();
    const float height = sapp_heightf();
    const float aspect = width / height;

    // use a fixed 'virtual resolution' for the spine rendering, but keep the same
    // aspect as the window/display
    const sspine_vec2 virt_size = { 1024.0f * aspect, 1024.0f };
    const sspine_layer_transform layer_transform = {
        .size = virt_size,
        .origin = { .x = virt_size.x * 0.5f, .y = virt_size.y * 0.5f }
    };

    sfetch_dowork();

    uint64_t start_time = stm_now();
    sspine_new_frame();
    for (int i = 0; i < NUM_INSTANCES; i++) {
        sspine_set_position(state.instances[i], state.grid[i].pos);
        sspine_update_instance(state.instances[i], delta_time);
        sspine_draw_instance_in_layer(state.instances[i], 0);
    }
    double eval_time = stm_ms(stm_since(start_time));

    sspine_context_info ctx_info = sspine_get_context_info(sspine_default_context());

    sdtx_canvas(sapp_widthf() * 0.25f, sapp_height() * 0.25f);
    sdtx_origin(2.0f, 2.0f);
    sdtx_home();
    sdtx_color3b(0, 0, 0);
    sdtx_printf("spine eval time:%.3fms\n", eval_time); sdtx_move_y(0.5f);
    sdtx_printf("vertices:%d indices:%d draws:%d", ctx_info.num_vertices, ctx_info.num_indices, ctx_info.num_commands);

    sg_begin_default_passf(&state.pass_action, width, height);
    sspine_draw_layer(0, &layer_transform);
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sfetch_shutdown();
    sspine_shutdown();
    __dbgui_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

// fetch callback for atlas data
static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.atlas = (load_status_t){
            .loaded = true,
            .data = (sspine_range) { .ptr = response->buffer_ptr, .size = response->fetched_size },
        };
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
            .data = (sspine_range) { .ptr = response->buffer_ptr, .size = response->fetched_size },
        };
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// load spine atlas image data and create a sokol-gfx image object
static void image_data_loaded(const sfetch_response_t* response) {
    const sspine_image_info* img_info = (sspine_image_info*)response->user_data;
    assert(img_info->valid);
    if (response->fetched) {
        // decode image via stb_image.h
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
            state.load_status.failed = true;
            sg_fail_image(img_info->image);
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
        sg_fail_image(img_info->image);
    }
}

// returns a xorshift32 random number between 0..<NUM_SKINS
static uint32_t random_skin_index(void) {
    static uint32_t x = 0x87654321;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return (x & (NUM_SKINS-1));
}

// called when both the atlas and skeleton file have been loaded
static void create_spine_objects(void) {
    
    // create spine atlas object
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = state.load_status.atlas.data
    });
    assert(sspine_atlas_valid(state.atlas));

    // create spine skeleton object
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .prescale = 0.15f,
        .anim_default_mix = 0.2f,
        .binary_data = state.load_status.skeleton.data,
    });
    assert(sspine_skeleton_valid(state.skeleton));

    // start loading atlas images
    const int num_images = sspine_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image_info img_info = sspine_get_image_info(state.atlas, img_index);
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .path = fileutil_get_path(img_info.filename, path_buf, sizeof(path_buf)),
            .channel = 0,
            .buffer_ptr = state.buffers.image,
            .buffer_size = sizeof(state.buffers.image),
            .callback = image_data_loaded,
            // sspine_image_info is small enough to directly fit into the user data payload
            .user_data_ptr = &img_info,
            .user_data_size = sizeof(img_info)
        });
    }

    // create many instances
    double initial_time = 0.0;
    for (int i = 0; i < NUM_INSTANCES; i++) {
        state.instances[i] = sspine_make_instance(&(sspine_instance_desc){
            .skeleton = state.skeleton,
        });
        assert(sspine_instance_valid(state.instances[i]));
        sspine_set_animation_by_name(state.instances[i], 0, (i & 1) ? "walk" : "dance", true);

        // create a skin set
        sspine_skinset skinset = sspine_make_skinset(&(sspine_skinset_desc){
            .skeleton = state.skeleton,
            .skins = {
                "skin-base",
                accessories[random_skin_index()],
                clothes[random_skin_index()],
                eyelids[random_skin_index()],
                eyes[random_skin_index()],
                hair[random_skin_index()],
                legs[random_skin_index()],
                nose[random_skin_index()]
            }
        });
        assert(sspine_skinset_valid(skinset));
        sspine_set_skinset(state.instances[i], skinset);
        sspine_update_instance(state.instances[i], initial_time);
        initial_time += 0.1;
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
        .high_dpi = true,
        .window_title = "spine-skinsets-sapp.c",
        .icon.sokol_default = true,
    };
}
