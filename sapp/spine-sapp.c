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
#include "dbgui/dbgui.h"

static struct {
    sspine_atlas atlas;
    sg_pass_action pass_action;
    struct {
        bool failed;
        int load_count;
        size_t atlas_size;
        size_t json_size;
    } load_status;
    struct {
        uint8_t atlas[8 * 1024];
        uint8_t json[256 * 1024];
        uint8_t png[256 * 1024];
    } buffers;
} state;

// sokol-fetch callback functions
static void atlas_loaded(const sfetch_response_t* response);
static void json_loaded(const sfetch_response_t* response);
static void image_loaded(const sfetch_response_t* response);

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
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f} }
    };

    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("spineboy.atlas", path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer_ptr = state.buffers.atlas,
        .buffer_size = sizeof(state.buffers.atlas),
        .callback = atlas_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("spineboy-pro.json", path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer_ptr = state.buffers.json,
        .buffer_size = sizeof(state.buffers.json),
        .callback = json_loaded,
    });
}

static void frame(void) {
    sfetch_dowork();
    if (state.load_status.load_count == 2) {
        state.load_status.load_count = 0;

        // create atlas from file data
        state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
            .data = {
                .ptr = state.buffers.atlas,
                .size = state.load_status.atlas_size,
            }
        });

        // asynchronously load images
        const int num_images = sspine_get_num_images(state.atlas);
        for (int img_index = 0; img_index < num_images; img_index++) {
            const sspine_image_info img_info = sspine_get_image_info(state.atlas, img_index);
            char path_buf[512];
            sfetch_send(&(sfetch_request_t){
                .channel = 0,
                .path = fileutil_get_path(img_info.filename, path_buf, sizeof(path_buf)),
                .buffer_ptr = state.buffers.png,
                .buffer_size = sizeof(state.buffers.png),
                .user_data_ptr = &img_info,
                .user_data_size = sizeof(img_info),
                .callback = image_loaded,
            });
        }
    }

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    (void)ev; // FIXME!
}

static void cleanup(void) {
    __dbgui_shutdown();
    sspine_shutdown();
    sfetch_shutdown();
    sg_shutdown();
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

static void atlas_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.load_count++;
        state.load_status.atlas_size = response->fetched_size;
    }
    else if (response->failed) {
    }   state.load_status.failed = true;
}

static void json_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.load_count++;
        state.load_status.json_size = response->fetched_size;
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

static void image_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        __builtin_printf("FIXME: create image\n");
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}
