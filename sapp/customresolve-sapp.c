//------------------------------------------------------------------------------
//  customresolve-sapp.c
//
//  Demonstrate custom MSAA resolve in a render pass which reads individual
//  MSAA samples in the fragment shader.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "customresolve-sapp.glsl.h"

#define WIDTH (160)
#define HEIGHT (120)

static struct {
    struct {
        sg_image img;
        sg_pipeline pip;
        sg_attachments atts;
        sg_pass_action action;
    } msaa;
    struct {
        sg_image img;
        sg_pipeline pip;
        sg_attachments atts;
        sg_pass_action action;
        sg_bindings bind;
    } resolve;
    struct {
        sg_pipeline pip;
        sg_pass_action action;
        sg_bindings bind;
    } display;
    sg_sampler smp; // a common non-filtering sampler
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // common objects
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // msaa-render-pass objects
    state.msaa.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = WIDTH,
        .height = HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 4,
        .label = "msaa image",
    });
    state.msaa.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(msaa_shader_desc(sg_query_backend())),
        .sample_count = 4,
        .depth.pixel_format = SG_PIXELFORMAT_NONE,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "msaa pipeline",
    });
    state.msaa.atts = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = state.msaa.img,
        .label = "msaa pass attachments",
    });
    state.msaa.action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .store_action = SG_STOREACTION_STORE,
            .clear_value = { 0, 0, 0, 1 },
        },
    };

    // resolve-render-pass objects
    state.resolve.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = WIDTH,
        .height = HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "resolve image",
    });
    state.resolve.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(resolve_shader_desc(sg_query_backend())),
        .sample_count = 1,
        .depth.pixel_format = SG_PIXELFORMAT_NONE,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "resolve pipeline",
    });
    state.resolve.atts = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = state.resolve.img,
        .label = "resolve pass attachments",
    });
    state.resolve.action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_DONTCARE,
            .store_action = SG_STOREACTION_STORE
        },
    };
    state.resolve.bind = (sg_bindings){
        .images[IMG_texms] = state.msaa.img,
        .samplers[SMP_smp] = state.smp,
    };

    // swapchain-render-pass objects
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label = "display pipeline",
    });
    state.display.action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_DONTCARE }
    };
    state.display.bind = (sg_bindings){
        .images[IMG_tex] = state.resolve.img,
        .samplers[SMP_smp] = state.smp,
    };
}

static void frame(void) {

    // draw a triangle into an msaa render target
    sg_begin_pass(&(sg_pass){ .action = state.msaa.action, .attachments = state.msaa.atts });
    sg_apply_pipeline(state.msaa.pip);
    sg_draw(0, 3, 1);
    sg_end_pass();

    // custom resolve pass (via a 'fullscreen triangle')
    sg_begin_pass(&(sg_pass){ .action = state.resolve.action, .attachments = state.resolve.atts });
    sg_apply_pipeline(state.resolve.pip);
    sg_apply_bindings(&state.resolve.bind);
    sg_draw(0, 3, 1);
    sg_end_pass();

    // the final swapchain pass (also via a 'fullscreen triangle')
    sg_begin_pass(&(sg_pass){ .action = state.display.action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
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
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 640,
        .height = 480,
        .sample_count = 1,
        .window_title = "customresolve-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
