//------------------------------------------------------------------------------
//  spine-skinsets-sapp.c
//  Test/demonstrate skinset usage and draw call merging.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_fetch.h"
#include "sokol_debugtext.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "spine/spine.h"
#include "sokol_spine.h"
#include "stb/stb_image.h"
#include "util/fileutil.h"
#include "dbgui/dbgui.h"

#define NUM_INSTANCES_X (16)
#define NUM_INSTANCES_Y (8)
#define NUM_INSTANCES (NUM_INSTANCES_X * NUM_INSTANCES_Y)
#define NUM_SKINS (8)
#define PRESCALE (0.15f)
#define GRID_DX (64.0f)
#define GRID_DY (96.0f)

typedef sspine_vec2 vec2;

typedef struct {
    bool loaded;
    sspine_range data;
} load_status_t;

typedef struct {
    vec2 pos;
    vec2 vec;
} grid_cell_t;

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instances[NUM_INSTANCES];
    sg_pass_action pass_action;
    float t;       // time interval 0..1
    uint32_t t_count;   // bumped each time t goes over 1
    grid_cell_t grid[NUM_INSTANCES];
    struct {
        load_status_t atlas;
        load_status_t skeleton;
        bool failed;
    } load_status;
    struct {
        uint8_t atlas[16 * 1024];
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
    // setup sokol-time
    stm_setup();
    // setup sokol-gfx
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    // setup sokol-debugtext
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric(),
        .logger.func = slog_func,
    });
    // setup sokol-spine
    sspine_setup(&(sspine_desc){
        .skinset_pool_size = NUM_INSTANCES,
        .instance_pool_size = NUM_INSTANCES,
        .max_vertices = 256 * 1024,
        .logger = {
            .func = slog_func,
        },
    });
    // setup sokol-fetch
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // pass action to clear to blue-ish
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };

    // start loading spine atlas file
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("mix-and-match-pma.atlas", path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer = SFETCH_RANGE(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });

    // start loading spine skeleton file
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("mix-and-match-pro.skel", path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer = SFETCH_RANGE(state.buffers.skeleton),
        .callback = skeleton_data_loaded,
    });

    // setup position- and motion-vector-grid
    const float dx = GRID_DX;
    const float dy = GRID_DY;
    float y = -dy * (NUM_INSTANCES_Y/2) + dy;
    for (int iy = 0; iy < NUM_INSTANCES_Y; iy++) {
        float x = -dx * (NUM_INSTANCES_X/2) + dx * 0.5f;
        for (int ix = 0; ix < NUM_INSTANCES_X; ix++) {
            grid_cell_t* cell = &state.grid[iy * NUM_INSTANCES_X + ix];
            if ((iy & 1) == 0) {
                cell->pos = (vec2){ x + ix*dx, y + iy*dy };
                if (ix == (NUM_INSTANCES_X - 1)) { cell->vec = (vec2){ 0.0f, 1.0f }; }
                else                             { cell->vec = (vec2){ 1.0f, 0.0f }; }
            }
            else {
                cell->pos = (vec2){ x + (NUM_INSTANCES_X-1-ix)*dx, y + iy*dy };
                if (ix == (NUM_INSTANCES_X - 1)) { cell->vec = (vec2){ 0.0f, 1.0f }; }
                else                             { cell->vec = (vec2){ -1.0f, 0.0f }; }
            }
        }
    }
}

static void frame(void) {
    const double delta_time = sapp_frame_duration();
    const float width = sapp_widthf();
    const float height = sapp_heightf();
    const float aspect = width / height;
    state.t += (float)delta_time;
    if (state.t > 1.0f) {
        state.t_count++;
        state.t -= 1.0f;
    }
    sfetch_dowork();

    // use a fixed 'virtual resolution' for the spine rendering, but keep the same
    // aspect as the window/display
    const vec2 virt_size = { 1024.0f * aspect, 1024.0f };
    const sspine_layer_transform layer_transform = {
        .size = virt_size,
        .origin = { .x = virt_size.x * 0.5f, .y = virt_size.y * 0.5f }
    };

    // update and draw Spine objects
    uint64_t start_time = stm_now();
    for (uint32_t i = 0; i < NUM_INSTANCES; i++) {
        const uint32_t grid_index = (i + state.t_count) % NUM_INSTANCES;
        const vec2 pos = state.grid[grid_index].pos;
        const vec2 vec = state.grid[grid_index].vec;
        vec2 p = {
            .x = pos.x + vec.x * GRID_DX * state.t,
            .y = pos.y + vec.y * GRID_DY * state.t,
        };
        sspine_set_position(state.instances[i], p);
        sspine_update_instance(state.instances[i], (float)delta_time);
        sspine_draw_instance_in_layer(state.instances[i], 0);
    }
    double eval_time = stm_ms(stm_since(start_time));

    // debug text
    sspine_context_info ctx_info = sspine_get_context_info(sspine_default_context());
    sdtx_canvas(sapp_widthf() * 0.25f, sapp_height() * 0.25f);
    sdtx_origin(2.0f, 2.0f);
    sdtx_home();
    sdtx_color3b(0, 0, 0);
    sdtx_printf("spine eval time:%.3fms\n", eval_time); sdtx_move_y(0.5f);
    sdtx_printf("vertices:%d indices:%d draws:%d", ctx_info.num_vertices, ctx_info.num_indices, ctx_info.num_commands);

    // actual sokol-gfx render pass
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
            .data = (sspine_range) { .ptr = response->data.ptr, .size = response->data.size },
        };
        // when both atlas and skeleton file have finished loading, create spine objects
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
            .data = (sspine_range) { .ptr = response->data.ptr, .size = response->data.size },
        };
        // when both atlas and skeleton file have finished loading, create spine objects
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
            // sokol-spine has already allocated a sokol-gfx image handle for use,
            // now "populate" the handle with an actual image
            sg_init_image(img_info.sgimage, &(sg_image_desc){
                .width = img_width,
                .height = img_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = img_info.min_filter,
                .mag_filter = img_info.mag_filter,
                .label = img_info.filename.cstr,
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

// returns a xorshift32 random number between 0..<NUM_SKINS
static uint32_t random_skin_index(void) {
    static uint32_t x = 0x87654321;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return (x & (NUM_SKINS-1));
}

// called when both the atlas and skeleton files have been loaded,
// creates an sspine_atlas and sspine_skeleton object, starts loading
// the atlas texture(s) and finally creates and sets up spine instances
static void create_spine_objects(void) {

    // create spine atlas object
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = state.load_status.atlas.data
    });
    assert(sspine_atlas_valid(state.atlas));

    // create spine skeleton object
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .prescale = PRESCALE,
        .anim_default_mix = 0.2f,
        .binary_data = state.load_status.skeleton.data,
    });
    assert(sspine_skeleton_valid(state.skeleton));

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

    // create many instances
    float initial_time = 0.0f;
    for (int i = 0; i < NUM_INSTANCES; i++) {
        state.instances[i] = sspine_make_instance(&(sspine_instance_desc){
            .skeleton = state.skeleton,
        });
        assert(sspine_instance_valid(state.instances[i]));
        const char* anim_name = (i & 1) ? "walk" : "dance";
        sspine_set_animation(state.instances[i], sspine_anim_by_name(state.skeleton, anim_name), 0, true);

        // create a skin set
        sspine_skinset skinset = sspine_make_skinset(&(sspine_skinset_desc){
            .skeleton = state.skeleton,
            .skins = {
                sspine_skin_by_name(state.skeleton, "skin-base"),
                sspine_skin_by_name(state.skeleton, accessories[random_skin_index()]),
                sspine_skin_by_name(state.skeleton, clothes[random_skin_index()]),
                sspine_skin_by_name(state.skeleton, eyelids[random_skin_index()]),
                sspine_skin_by_name(state.skeleton, eyes[random_skin_index()]),
                sspine_skin_by_name(state.skeleton, hair[random_skin_index()]),
                sspine_skin_by_name(state.skeleton, legs[random_skin_index()]),
                sspine_skin_by_name(state.skeleton, nose[random_skin_index()])
            }
        });
        assert(sspine_skinset_valid(skinset));
        sspine_set_skinset(state.instances[i], skinset);
        sspine_update_instance(state.instances[i], initial_time);
        initial_time += 0.1f;
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
