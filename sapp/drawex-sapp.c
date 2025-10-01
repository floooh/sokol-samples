//------------------------------------------------------------------------------
//  drawex-sapp.c
//
//  Demonstrate/test sg_draw_ex() with base-vertex and base-index.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#include "sokol_color.h"
#include "dbgui/dbgui.h"

typedef enum {
    BASE_VERTEX = (1<<0),
    BASE_INSTANCE = (1 << 1),
    BASE_VERTEX_INSTANCE = (1 << 2),
} draw_mode_t;

static struct {
    sg_pass_action pass_action;
    uint8_t supported_modes; // bitmask of draw_mode_t
    draw_mode_t current_mode;
} state = {
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = sg_maroon,
        }
    }
};

static void draw_panel(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_c64(),
        .logger.func = slog_func,
    });

    // create a bit mask of supported draw modes
    if (sg_query_features().draw_base_vertex) {
        state.supported_modes |= BASE_VERTEX;
    }
    if (sg_query_features().draw_base_instance) {
        state.supported_modes |= BASE_INSTANCE;
    }
    if (sg_query_features().draw_base_vertex_base_instance) {
        state.supported_modes |= BASE_VERTEX_INSTANCE;
    }
    state.current_mode = BASE_VERTEX;
}

static void frame(void) {
    draw_panel();

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        switch (ev->key_code) {
            case SAPP_KEYCODE_1: state.current_mode = BASE_VERTEX; break;
            case SAPP_KEYCODE_2: state.current_mode = BASE_INSTANCE; break;
            case SAPP_KEYCODE_3: state.current_mode = BASE_VERTEX_INSTANCE; break;
            default: break;
        }
    }
    __dbgui_event(ev);
}

static void cleanup(void) {
    sdtx_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

static void draw_panel(void) {
    sdtx_canvas(sapp_width() * 0.5f, sapp_height() * 0.5f);
    sdtx_origin(1.0f, 2.0f);
    sdtx_home();
    if (state.supported_modes != 0) {
        sdtx_printf("Press keys 1,2,3:\n\n");
    }
    if (state.supported_modes & BASE_VERTEX) {
        sdtx_printf("%c (1) draw with base vertex\n", (state.current_mode == BASE_VERTEX) ? '*':' ');
    } else {
        sdtx_puts("!!! base vertex not supported\n");
    }
    if (state.supported_modes & BASE_INSTANCE) {
        sdtx_printf("%c (2) draw with base instance\n", (state.current_mode == BASE_INSTANCE) ? '*':' ');
    } else {
        sdtx_puts("!!! base instance not supported\n");
    }
    if (state.supported_modes & BASE_VERTEX_INSTANCE) {
        sdtx_printf("%c (3) draw with base vertex + base instance\n", (state.current_mode == BASE_VERTEX_INSTANCE) ? '*':' ');
    } else {
        sdtx_puts("!!! base vertex + base instance not supported\n");
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .window_title = "drawex-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
