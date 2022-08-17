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
        bool atlas_loaded;
        bool json_loaded;
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
    if (state.load_status.atlas_loaded && state.load_status.json_loaded) {
        // FIXME: create atlas and skeleton
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
        state.load_status.atlas_loaded = true;
    }
    else if (response->failed) {
    }   state.load_status.failed = true;
}

static void json_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.json_loaded = true;
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}
