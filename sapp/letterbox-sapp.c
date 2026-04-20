//------------------------------------------------------------------------------
//  letterbox-sapp.c
//
//  Demonstrate/test sokol_letterbox.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#define SOKOL_LETTERBOX_IMPL
#include "sokol_letterbox.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#define NUM_ANCHORS (5)

static struct {
    sg_pass_action pass_action;
    slbx_letterbox_desc lbox;
    bool link_lr_border;
    bool link_tb_border;
    int cur_anchor_idx;
    struct {
        slbx_anchor anchor;
        const char* label;
    } anchors[NUM_ANCHORS];
} state = {
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0, 0, 0, 1 },
        },
    },
    .link_lr_border = true,
    .link_tb_border = true,
    .lbox.content_aspect_ratio = 4.0f / 3.0f,
    .anchors = {
        { .anchor = SLBX_ANCHOR_CENTER, .label = "Center" },
        { .anchor = SLBX_ANCHOR_TOP, .label = "Top" },
        { .anchor = SLBX_ANCHOR_BOTTOM, .label = "Bottom" },
        { .anchor = SLBX_ANCHOR_LEFT, .label = "Left" },
        { .anchor = SLBX_ANCHOR_RIGHT, .label = "Right" },
    },
};

static void draw_ui(void);
static void main_quad(void);
static void corner_quad(float x, float y);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
}

static void frame(void) {
    draw_ui();
    const int width = sapp_width();
    const int height = sapp_height();

    // draw a letterboxed fullscreen quad via sgl
    sgl_defaults();
    const slbx_viewport vp = slbx_letterbox_viewport(width, height, &state.lbox);
    sgl_viewport(vp.x, vp.y, vp.width, vp.height, true);
    sgl_begin_quads();
    main_quad();
    corner_quad(-0.9f, +0.9f);
    corner_quad(+0.9f, +0.9f);
    corner_quad(+0.9f, -0.9f);
    corner_quad(-0.9f, -0.9f);
    sgl_end();
    sgl_viewport(0, 0, width, height, true);

    // render everything in a sokol-gfx pass
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sgl_draw();
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void main_quad(void) {
    sgl_v2f_c3b(-1.0f, +1.0f, 255, 0, 0);
    sgl_v2f_c3b(+1.0f, +1.0f, 255, 255, 0);
    sgl_v2f_c3b(+1.0f, -1.0f, 0, 255, 0);
    sgl_v2f_c3b(-1.0f, -1.0f, 0, 255, 255);
}

static void corner_quad(float x, float y) {
    float s = 0.05f;
    uint8_t r = 255;
    uint8_t g = 128;
    uint8_t b = 255;
    sgl_v2f_c3b(x - s, y + s, r, g, b);
    sgl_v2f_c3b(x + s, y + s, r, g, b);
    sgl_v2f_c3b(x + s, y - s, r, g, b);
    sgl_v2f_c3b(x - s, y - s, r, g, b);
}

static void cleanup(void) {
    simgui_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

static const char* anchor_getter(void* userdata, int index) {
    assert((index >= 0) && (index < NUM_ANCHORS));
    (void)userdata;
    return state.anchors[index].label;
}

static void draw_ui(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        igText("Resize app window!\n");
        igSliderFloat("Content Aspect Ratio", &state.lbox.content_aspect_ratio, 0.5f, 2.0f);
        if (igComboCallback("Anchor", &state.cur_anchor_idx, anchor_getter, 0, NUM_ANCHORS)) {
            state.lbox.anchor = state.anchors[state.cur_anchor_idx].anchor;
        }
        igSeparatorText("Border");
        igCheckbox("Link Left/Right", &state.link_lr_border);
        igCheckbox("Link Top/Bottom", &state.link_tb_border);
        if (igSliderInt("Left", &state.lbox.border.left, -50, 50) && state.link_lr_border) {
            state.lbox.border.right = state.lbox.border.left;
        }
        if (igSliderInt("Right", &state.lbox.border.right, -50, 50) && state.link_lr_border) {
            state.lbox.border.left = state.lbox.border.right;
        }
        if (igSliderInt("Top", &state.lbox.border.top, -50, 50) && state.link_tb_border) {
            state.lbox.border.bottom = state.lbox.border.top;
        }
        if (igSliderInt("Bottom", &state.lbox.border.bottom, -50, 50) && state.link_tb_border) {
            state.lbox.border.top = state.lbox.border.bottom;
        }
    }
    igEnd();
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
        .window_title = "letterbox-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
