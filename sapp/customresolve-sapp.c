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

static struct {
    struct {
        sg_image img;
        sg_pipeline pip;
    } msaa;
    struct {
        sg_image img;
        sg_pipeline pip;
        sg_attachments att;
    } resolve;
    struct {
        sg_pipeline pip;
    } display;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    state.msaa.img = sg_make_image(&(sg_image_desc){
        .width = 160,
        .height = 120,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 4,
        .label = "msaa image",
    });

    state.resolve.img = sg_make_image(&(sg_image_desc){
        .width = 160,
        .height = 120,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "resolve image",
    });

    state.msaa.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(msaa_shader_desc(sg_query_backend())),
        .sample_count = 4,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "msaa pipeline",
    });

    state.resolve.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(resolve_shader_desc(sg_query_backend())),
        .sample_count = 1,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "resolve pipeline",
    });

    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label = "display pipeline",
    });
}

static void frame(void) {

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
