//------------------------------------------------------------------------------
//  srgb-sapp.c
//
//  Test rendering into an SRGB framebuffer. This is a modified copy
//  of triangle-bufferless-sapp.c
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "srgb-sapp.glsl.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"

static struct {
    sg_pipeline pip;
    sg_pass_action pass_action;
} state;

static void print_webgl2_note(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();
    sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_oric(), .logger.func = slog_func });

    sg_shader shd = sg_make_shader(triangle_shader_desc(sg_query_backend()));
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){ .shader = shd });
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1} },
    };
}

static void frame(void) {
    print_webgl2_note();
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_draw(0, 3, 1);
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

static void print_webgl2_note(void) {
    #if defined(__EMSCRIPTEN__)
    if (sg_query_backend() == SG_BACKEND_GLES3) {
        sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
        sdtx_origin(0.5f, 2.0f);
        sdtx_puts("NOTE: SRGB framebuffers not supported on WebGL2");
    }
    #endif
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
        .srgb = true,   // NOTE: request SRGB framebuffer
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .window_title = "srgb-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
