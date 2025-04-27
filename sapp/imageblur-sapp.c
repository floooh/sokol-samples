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
#include <math.h> // ceilf()

static struct {
    int src_width;
    int src_height;
    sg_sampler smp;
    struct {
        sg_pipeline pip;
        sg_image src_img;
        sg_image storage_img[2];
        sg_attachments atts[2];
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

    // create a non-filtering sampler
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
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

    // first blur pass reads src image and writes to first storage image
    const int batch = 4;
    const int tile_dim = 128;
    cs_params_t cs_params = {
        .filter_size = state.ui.filter_size,
        .block_dim = tile_dim - (state.ui.filter_size - 1),
    };
    {
        cs_params.flip = 0;
        const int num_workgroups_x = (int)ceilf((float)state.src_width / (float)cs_params.block_dim);
        const int num_workgroups_y = (int)ceilf((float)state.src_height / (float)batch);
        sg_begin_pass(&(sg_pass){ .compute = true, .attachments = state.compute.atts[0], .label = "blur-pass-0"});
        sg_apply_pipeline(state.compute.pip);
        sg_apply_bindings(&(sg_bindings){
            .images[IMG_inp_tex] = state.compute.src_img,
            .samplers[SMP_smp] = state.smp,
        });
        sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
        sg_dispatch(num_workgroups_x, num_workgroups_y, 1);
        sg_end_pass();
    }
    // second blur pass reads first storage image and writes into second storage image
    {
        cs_params.flip = 1;
        const int num_workgroups_x = (int)ceilf((float)state.src_height / (float)cs_params.block_dim);
        const int num_workgroups_y = (int)ceilf((float)state.src_width / (float)batch);
        sg_begin_pass(&(sg_pass){ .compute = true, .attachments = state.compute.atts[1], .label = "blur-pass-1"});
        sg_apply_pipeline(state.compute.pip);
        sg_apply_bindings(&(sg_bindings){
            .images[IMG_inp_tex] = state.compute.storage_img[0],
            .samplers[SMP_smp] = state.smp,
        });
        sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
        sg_dispatch(num_workgroups_x, num_workgroups_y, 1);
        sg_end_pass();
    }
    // repeat ping-ponging between storage images
    for (int i = 0; i < state.ui.iterations - 1; i++) {
        // read from second storage image, write into first storage image
        {
            cs_params.flip = 0;
            const int num_workgroups_x = (int)ceilf((float)state.src_width / (float)cs_params.block_dim);
            const int num_workgroups_y = (int)ceilf((float)state.src_height / (float)batch);
            sg_begin_pass(&(sg_pass){ .compute = true, .attachments = state.compute.atts[0], .label = "blur-pass-3"});
            sg_apply_pipeline(state.compute.pip);
            sg_apply_bindings(&(sg_bindings){
                .images[IMG_inp_tex] = state.compute.storage_img[1],
                .samplers[SMP_smp] = state.smp,
            });
            sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
            sg_dispatch(num_workgroups_x, num_workgroups_y, 1);
            sg_end_pass();
        }
        // read from first storage image, write into second storage image
        {
            cs_params.flip = 1;
            const int num_workgroups_x = (int)ceilf((float)state.src_height / (float)cs_params.block_dim);
            const int num_workgroups_y = (int)ceilf((float)state.src_width / (float)batch);
            sg_begin_pass(&(sg_pass){ .compute = true, .attachments = state.compute.atts[1], .label = "blur-pass-4"});
            sg_apply_pipeline(state.compute.pip);
            sg_apply_bindings(&(sg_bindings){
                .images[IMG_inp_tex] = state.compute.storage_img[0],
                .samplers[SMP_smp] = state.smp,
            });
            sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
            sg_dispatch(num_workgroups_x, num_workgroups_y, 1);
            sg_end_pass();
        }
    }

    // FIXME: display the result
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain(), .label = "display-pass" });
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
            state.compute.src_img = sg_make_image(&(sg_image_desc){
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
                state.compute.storage_img[i] = sg_make_image(&(sg_image_desc){
                    .usage = {
                        .storage_attachment = true,
                    },
                    .width = state.src_width,
                    .height = state.src_height,
                    .pixel_format = SG_PIXELFORMAT_RGBA8,
                    .label = storage_image_labels[i],
                });
                state.compute.atts[i] = sg_make_attachments(&(sg_attachments_desc){
                    .storages[SIMG_outp_tex] = { .image = state.compute.storage_img[i] },
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
