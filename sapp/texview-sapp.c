//------------------------------------------------------------------------------
//  texview-sapp.c
//
//  Test/demonstrate texture-view features.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "sokol_color.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#include "dbgui/dbgui.h"
#include "basisu/sokol_basisu.h"
#include "util/fileutil.h"
#include "texview-sapp.glsl.h"

#define MAX_FILE_SIZE (128 * 1024)
#define NUM_FILES (5)
static const char* files[NUM_FILES] = {
    "kodim05.basis",
    "kodim07.basis",
    "kodim17.basis",
    "kodim20.basis",
    "kodim23.basis",
};

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sg_view tex_view;
    sg_pipeline pip;
    sg_sampler smp;
    struct {
        int width;
        int height;
        int num_mipmaps;
    } img_info;
    struct {
        bool pending;
        bool failed;
    } load;
    struct {
        int selected;
    } ui;
} state;

static uint8_t file_buffer[MAX_FILE_SIZE];

static void draw_ui(void);
static void fetch_async(const char* filename);
static void fetch_callback(const sfetch_response_t*);
static void reinit_texview(void);

static void init(void) {
    sbasisu_setup();
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 1,
        .num_channels = 1,
        .num_lanes = 1,
        .logger.func = slog_func,
    });
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = SG_STEEL_BLUE },
    };

    state.img = sg_alloc_image();
    state.tex_view = sg_alloc_view();

    // a render pipeline for bufferless 2D-rendering
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(texview_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "pipeline",
    });

    // a linear-filtering sampler
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_LINEAR,
        .label = "sampler",
    });

    // start loading the first file
    fetch_async(files[0]);
}

static void frame(void) {
    sfetch_dowork();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });

    draw_ui();

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_tex] = state.tex_view,
        .samplers[SMP_smp] = state.smp,
    });
    sg_draw(0, 4, 1);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

static void cleanup(void) {
    sfetch_shutdown();
    simgui_shutdown();
    sg_shutdown();
    sbasisu_shutdown();
}

static void draw_ui(void) {
    igSetNextWindowPos((ImVec2){ 20, 20 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        if (state.load.pending) {
            igText("Loading ...");
        } else {
            if (state.load.failed) {
                igText("Loading failed!");
            }
            if (igComboChar("Image", &state.ui.selected, files, IM_ARRAYSIZE(files))) {
                fetch_async(files[state.ui.selected]);
            }
            igText("Width:   %d", state.img_info.width);
            igText("Height:  %d", state.img_info.height);
            igText("Mipmaps: %d", state.img_info.num_mipmaps);
        }
    }
    igEnd();
}

static void fetch_async(const char* filename) {
    state.load.pending = false;
    state.load.failed = false;
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path(filename, path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(file_buffer),
    });
}

static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load.pending = false;
        sg_uninit_image(state.img);
        const sg_image_desc img_desc = sbasisu_transcode((sg_range){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
        state.img_info.width = img_desc.width;
        state.img_info.height = img_desc.height;
        state.img_info.num_mipmaps = img_desc.num_mipmaps;
        sg_init_image(state.img, &img_desc);
        sbasisu_free(&img_desc);
        reinit_texview();
    } else if (response->failed) {
        state.load.failed = true;
    }
}

static void reinit_texview(void) {
    sg_uninit_view(state.tex_view);
    sg_init_view(state.tex_view, &(sg_view_desc){
        .texture = {
            .image = state.img,
            // FIXME: mip-range
        },
    });
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
        .window_title = "texview-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
