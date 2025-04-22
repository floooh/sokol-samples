//------------------------------------------------------------------------------
//  imageblur-sapp.c
//
//  Image-blur running in a compute shader writing to a storage texture. Also
//  demonstrates using shader-shared memory and control-flow barriers.
//
//  Ported from WebGPU sample: https://webgpu.github.io/webgpu-samples/?sample=imageBlur
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#include "stb/stb_image.h"
#include "util/fileutil.h"
#include "imageblur-sapp.glsl.h"

static struct {
    cs_params_t cs_params;
    struct {
        sg_image src_img;
        sg_image dst_img;
        sg_pipeline pip;
        sg_attachments atts;
    } compute;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
    struct {
        int filter_size;
        int iterations;
    } ui;
    struct {
        bool succeeded;
        bool failed;
    } io;
} state = {
    .display.pass_action = {
        // FIXME: SG_LOADACTION_DONTCARE
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1 } },
    },
    .ui = {
        .filter_size = 15,
        .iterations = 2,
    },
};
static sgimgui_t sgimgui;
static uint8_t file_buffer[256 * 1024];

static void fetch_callback(const sfetch_response_t* response);
static void draw_ui(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgimgui_init(&sgimgui, &(sgimgui_desc_t){0});
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 1,
        .num_channels = 1,
        .num_lanes = 1,
        .logger.func = slog_func,
    });

    // start loading source png file asynchronously, all sokol-gfx image objects
    // and an attachments object will be created when loading has finished
    static char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("baboon.png", path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(file_buffer),
    });

    // create compute shader and pipeline
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(compute_shader_desc(sg_query_backend())),
        .label = "compute-pipeline",
    });

}

static void frame(void) {
    sfetch_dowork();
    draw_ui();

    // if loading hasn't finished yet or has failed, just draw a fallback ui
    if (!state.io.succeeded) {
        sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
        simgui_render();
        sg_end_pass();
        sg_commit();
        return;
    }

    // loading has finished
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sfetch_shutdown();
    sgimgui_discard(&sgimgui);
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

// called when texture file has finished loading, this creates a
// regular image object with the source pixels, and a storage attachment
// image object which will be written by the compute shader
static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        int png_width, png_height, num_channels;
        const int desired_channels = 4;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &png_width, &png_height,
            &num_channels, desired_channels
        );
        if (pixels) {
            // the source image is a regular texture
            state.compute.src_img = sg_make_image(&(sg_image_desc){
                .width = png_width,
                .height = png_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(png_width * png_height * 4),
                },
                .label = "source-image",
            });
            // the destination image is a storage texture
            state.compute.dst_img = sg_make_image(&(sg_image_desc){
                .usage = {
                    .storage_attachment = true,
                },
                .width = png_width,
                .height = png_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .label = "storage-image",
            });
            // an attachments object with the storage texture at the bindslot expected by
            // the compute shader
            state.compute.atts = sg_make_attachments(&(sg_attachments_desc){
                .storages[SIMG_outp_tex] = { .image = state.compute.dst_img },
            });
            state.io.succeeded = true;
        }
    } else if (response->failed) {
        state.io.failed = true;
    }
}

static void draw_ui(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu(&sgimgui, "sokol-gfx");
        igEndMainMenuBar();
    }
    igSetNextWindowBgAlpha(0.8f);
    igSetNextWindowPos((ImVec2){10,30}, ImGuiCond_Once);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing;
    if (igBegin("controls", 0, flags)) {
        if (!state.io.succeeded && !state.io.failed) {
            igText("Loading...");
        } else if (state.io.failed) {
            igText("Failed to load source texture!");
        } else {
            igSliderInt("Filter Size", &state.ui.filter_size, 2, 34);
            igSliderInt("Iterations", &state.ui.iterations, 1, 10);
        }
    }
    igEnd();
    sgimgui_draw(&sgimgui);
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
        .window_title = "imageblur-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
