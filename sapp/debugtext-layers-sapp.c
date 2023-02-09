//------------------------------------------------------------------------------
//  debugtext-layers-sapp.c
//  Demonstrates layered rendering with sokol_debugtext.h and sokol_gl.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "dbgui/dbgui.h"

#define NUM_LAYERS (3)

static struct {
    sg_pass_action pass_action;
    sgl_pipeline sgl_pip;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_cpc(),
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func
    });

    // pass action for clearing to black
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // a sokol-gl pipeline with alpha blending enabled
    state.sgl_pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .depth.write_enabled = false,
        .colors[0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            }
        }
    });
}

static void frame(void) {

    // render debugtext into layers
    sdtx_canvas(64.0f, 48.0f);
    sdtx_font(0);
    sdtx_color3b(255, 255, 255);
    sdtx_home();
    for (int i = 0; i < NUM_LAYERS; i++) {
        sdtx_layer(i);
        sdtx_pos(0.5f, 0.5f + 2.0f * (float)i);
        sdtx_printf("Layer %d", i);
    }

    // render horizontal bars into layers via sokol-gl
    const float h = 2.0f / (float)NUM_LAYERS;
    sgl_defaults();
    sgl_load_pipeline(state.sgl_pip);
    for (int i = 0; i <= NUM_LAYERS; i++) {
        const float y0 = (1.0f - i * h) + h * 0.5f;
        const float y1 = y0 - h;
        sgl_layer(i);
        switch (i) {
            case 0: sgl_c4b(255, 0, 0, 160); break;
            case 1: sgl_c4b(0, 255, 0, 160); break;
            case 2: sgl_c4b(0, 0, 255, 160); break;
            default: sgl_c4b(255, 0, 0, 160); break;
        }
        sgl_begin_quads();
        sgl_v2f(-1.0f, y0); sgl_v2f(+1.0f, y0);
        sgl_v2f(+1.0f, y1); sgl_v2f(-1.0f, y1);
        sgl_end();
    }

    // actually render everything in an sokol-gfx render pass
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    for (int i = 0; i < NUM_LAYERS; i++) {
        sgl_draw_layer(i);
        sdtx_draw_layer(i);
    }
    sgl_draw_layer(NUM_LAYERS);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sgl_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .window_title = "debugtext-layers-sapp",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}