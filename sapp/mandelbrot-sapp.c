//------------------------------------------------------------------------------
//  mandelbrot-sapp.c
//
//  Render Mandelbrot fractal in pixel shader. Ported back to C
//  from the sokol_mandelbrot.mc sample in https://minc.dev/
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "mandelbrot-sapp.glsl.h"
#include "dbgui/dbgui.h"
#include <math.h>

static struct {
    sg_pipeline pip;
    float time;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(mandelbrot_shader_desc(sg_query_backend())),
    });
}

static void frame(void) {
    // loop time to prevent a too deep mandelbrot zoom
    state.time = fmodf(state.time + (float)sapp_frame_duration(), 20.0);
    float aspect = sapp_widthf() / sapp_heightf();

    // number of iterations grows with zoom level, max 256
    const float width = 4.0f * powf(0.6f, (float)state.time);
    int iter_max = 64 + (int)(log2f(4.0f / width) * 32.0f);
    if (iter_max > 256) {
        iter_max = 256;
    }
    const fs_params_t fs_params = {
        .time = (float)state.time,
        .width = width,
        .iter_max = iter_max,
        .aspect = {
            [0] = aspect >= 1.0f ? aspect : 1.0f,
            [1] = aspect >= 1.0f ? 1.0f : 1.0f / aspect,
        },
    };

    // rendering happens via a 'fullscreen triangle' synthesized in the vertex shader
    sg_begin_pass(&(sg_pass){
        .action.colors[0].load_action = SG_LOADACTION_DONTCARE,
        .swapchain = sglue_swapchain()
    });
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(fs_params));
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
        .width = 512,
        .height = 512,
        .window_title = "mandelbrot-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
