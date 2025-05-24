//------------------------------------------------------------------------------
//  write-storageimage-sapp.c
//  A simplest possible sample to write image data with a compute shader.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "write-storageimage-sapp.glsl.h"
#include <math.h>

#define WIDTH (256)
#define HEIGHT (256)

static struct {
    double time;
    struct {
        sg_image img;
        sg_attachments atts;
        sg_pipeline pip;
    } compute;
    struct {
        sg_pipeline pip;
        sg_sampler smp;
        sg_pass_action pass_action;
    } display;
} state = {
    .display.pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_DONTCARE },
    },
};

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // an image object with storage attachment usage
    state.compute.img = sg_make_image(&(sg_image_desc){
        .usage.storage_attachment = true,
        .width = 256,
        .height = 256,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "storage-image",
    });

    // a pass attachment object with the image as storage attachment
    state.compute.atts = sg_make_attachments(&(sg_attachments_desc){
        .storages[SIMG_cs_out_tex] = { .image = state.compute.img },
        .label = "storage-attachments",
    });

    // a compute pipeline object with the compute shader
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(compute_shader_desc(sg_query_backend())),
        .label = "compute-pipeline",
    });

    // a shader and pipeline for a textured 'fullscreen-triangle' with
    // vertex positions synthesized in the vertex shader, the default
    // pipeline state is sufficient for rendering a 2D triangle
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label = "display-pipeline",
    });

    // a sampler needed for sampling the storage image as texture
    state.display.smp = sg_make_sampler(&(sg_sampler_desc){
        .mag_filter = SG_FILTER_LINEAR,
        .min_filter = SG_FILTER_LINEAR,
        .label = "display-sampler",
    });
}

static void frame(void) {
    state.time += sapp_frame_duration();

    // a value that fluctuates between 0 and 1
    const double time_offset = (sin(state.time * 4.0) + 1.0) * 0.5;

    // run compute shader to update the storage image
    const cs_params_t cs_params = { .offset = (float)time_offset };
    sg_begin_pass(&(sg_pass){ .compute = true, .attachments = state.compute.atts, .label = "compute-pass" });
    sg_apply_pipeline(state.compute.pip);
    sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
    sg_dispatch(WIDTH / 16, HEIGHT / 16, 1);    // shader local_size_x/y is 16
    sg_end_pass();

    // and a swapchain pass to render the result
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain(), .label = "render-pass" });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .images[IMG_disp_tex] = state.compute.img,
        .samplers[SMP_disp_smp] = state.display.smp,
    });
    sg_draw(0, 3, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 512,
        .height = 512,
        .window_title = "write-storageimage-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
