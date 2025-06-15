//------------------------------------------------------------------------------
//  imageblur-sapp.c
//
//  Image-blur running in a compute shader writing to a storage texture. Also
//  demonstrates using shader-shared memory and shader barriers.
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
#include <math.h> // ceilf()

static struct {
    int src_width;
    int src_height;
    sg_sampler smp;
    struct {
        sg_pipeline pip;
        sg_image src_image;
        sg_image storage_image[2];
        sg_attachments attachments[2];
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
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1 } },
    },
    .ui = {
        .filter_size = 1,
        .iterations = 2,
    },
};
static sgimgui_t sgimgui;
static uint8_t file_buffer[256 * 1024];

static void blur_pass(int flip, sg_attachments pass_attachments, sg_image src_image);
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

    // create a non-filtering sampler
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    // start loading source png file asynchronously, all sokol-gfx image
    // and attachemnts objects will be created when loading has finished
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

    // a shader and pipeline to display the result (we'll
    // synthesize the fullscreen vertices in the vertex shader so we
    // don't need any buffers or a pipeline vertex layout, and default
    // render state is fine for rendering a 2D triangle
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label = "display-pipeline",
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

    // ping-pong blur passes starting with the source image
    blur_pass(0, state.compute.attachments[0], state.compute.src_image);
    blur_pass(1, state.compute.attachments[1], state.compute.storage_image[0]);
    for (int i = 0; i < state.ui.iterations - 1; i++) {
        blur_pass(0, state.compute.attachments[0], state.compute.storage_image[1]);
        blur_pass(1, state.compute.attachments[1], state.compute.storage_image[0]);
    }

    // swapchain render pass to display the result
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain(), .label = "display-pass" });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .images[IMG_disp_tex] = state.compute.storage_image[1],
        .samplers[SMP_disp_smp] = state.smp,
    });
    sg_draw(0, 3, 1);
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

// perform a horizontal or vertical blur pass in a compute shader
void blur_pass(int flip, sg_attachments pass_attachments, sg_image src_image) {
    const int batch = 4;        // must match shader
    const int tile_dim = 128;   // must match shader
    const int filter_size = state.ui.filter_size | 1;   // must be odd
    const cs_params_t cs_params = {
        .flip = flip,
        .filter_dim = filter_size,
        .block_dim = tile_dim - (filter_size - 1),
    };
    const float src_width = flip ? state.src_height : state.src_width;
    const float src_height = flip ? state.src_width : state.src_height;
    const int num_workgroups_x = (int)ceilf(src_width / (float)cs_params.block_dim);
    const int num_workgroups_y = (int)ceilf(src_height / (float)batch);

    sg_begin_pass(&(sg_pass){ .compute = true, .attachments = pass_attachments, .label = "blur-pass"});
    sg_apply_pipeline(state.compute.pip);
    sg_apply_bindings(&(sg_bindings){
        .images[IMG_cs_inp_tex] = src_image,
        .samplers[SMP_cs_smp] = state.smp,
    });
    sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
    sg_dispatch(num_workgroups_x, num_workgroups_y, 1);
    sg_end_pass();
}

// called when texture file has finished loading, this creates a
// regular image object with the source pixels, and a storage attachment
// image object which will be written by the compute shader
static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        int num_channels;
        const int desired_channels = 4;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &state.src_width, &state.src_height,
            &num_channels, desired_channels
        );
        if (pixels) {
            // the source image is a regular texture
            state.compute.src_image = sg_make_image(&(sg_image_desc){
                .width = state.src_width,
                .height = state.src_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(state.src_width * state.src_height * 4),
                },
                .label = "source-image",
            });
            // create two storage textures for the 2-pass blur and two attachments objects
            const char* storage_image_labels[2] = { "storage-image-0", "storage-image-1" };
            const char* compute_pass_labels[2] = { "compute-pass-0", "compute-pass-1" };
            for (int i = 0; i < 2; i++) {
                state.compute.storage_image[i] = sg_make_image(&(sg_image_desc){
                    .usage = {
                        .storage_attachment = true,
                    },
                    .width = state.src_width,
                    .height = state.src_height,
                    .pixel_format = SG_PIXELFORMAT_RGBA8,
                    .label = storage_image_labels[i],
                });
                state.compute.attachments[i] = sg_make_attachments(&(sg_attachments_desc){
                    .storages[SIMG_cs_outp_tex] = { .image = state.compute.storage_image[i] },
                    .label = compute_pass_labels[i],
                });
            }
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
            igSliderInt("Filter Size", &state.ui.filter_size, 1, 33);
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
