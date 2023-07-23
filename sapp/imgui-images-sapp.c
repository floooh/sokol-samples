//------------------------------------------------------------------------------
//  imgui-images-sapp.c
//
//  Demonstrates how to use sokol-gfx images and sampler with Dear ImGui
//  via sokol_imgui.h
//
//  Uses sokol-gl to render into offscreen render target textures, and then
//  renders the render target texture with different samplers in Dear ImGui.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

#define OFFSCREEN_WIDTH (32)
#define OFFSCREEN_HEIGHT (32)
#define OFFSCREEN_COLOR_FORMAT (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_DEPTH_FORMAT (SG_PIXELFORMAT_DEPTH)
#define OFFSCREEN_SAMPLE_COUNT (1)

static struct {
    double angle_deg;
    struct {
        sgl_context sgl_ctx;
        sgl_pipeline sgl_pip;
        sg_image color_img;
        sg_image depth_img;
        sg_pass pass;
        sg_pass_action pass_action;
    } offscreen;
    struct {
        sg_pass_action pass_action;
    } display;
    struct {
        simgui_image_t img_nearest_clamp;
        simgui_image_t img_linear_clamp;
        simgui_image_t img_nearest_repeat;
        simgui_image_t img_linear_mirror;
    } ui;
} state;

static void draw_cube(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func,
    });

    // a sokol-gl context to render into an offscreen pass
    state.offscreen.sgl_ctx = sgl_make_context(&(sgl_context_desc_t){
        .color_format = OFFSCREEN_COLOR_FORMAT,
        .depth_format = OFFSCREEN_DEPTH_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // a sokol-gl pipeline object for 3D rendering
    state.offscreen.sgl_pip = sgl_context_make_pipeline(state.offscreen.sgl_ctx, &(sg_pipeline_desc){
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .pixel_format = OFFSCREEN_DEPTH_FORMAT,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .colors[0].pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // a color and depth render target textures for the offscreen pass
    state.offscreen.color_img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });
    state.offscreen.depth_img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_DEPTH_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // a render pass
    state.offscreen.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = state.offscreen.color_img,
        .depth_stencil_attachment.image = state.offscreen.depth_img,
    });

    // a pass-action for the offscreen pass which clears to black
    state.offscreen.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    // a pass action for the default pass which clears to blue-ish
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f } },
    };

    // sokol-imgui image-sampler-pair wrappers with different sampler types
    state.ui.img_nearest_clamp = simgui_make_image(&(simgui_image_desc_t){
        .image = state.offscreen.color_img,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        })
    });
    state.ui.img_linear_clamp = simgui_make_image(&(simgui_image_desc_t){
        .image = state.offscreen.color_img,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        })
    });
    state.ui.img_nearest_repeat = simgui_make_image(&(simgui_image_desc_t){
        .image = state.offscreen.color_img,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_REPEAT,
        })
    });
    state.ui.img_linear_mirror = simgui_make_image(&(simgui_image_desc_t){
        .image = state.offscreen.color_img,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_MIRRORED_REPEAT,
            .wrap_v = SG_WRAP_MIRRORED_REPEAT,
        })
    });
}

static void frame(void) {
    state.angle_deg += sapp_frame_duration() * 60.0;
    const float a = sgl_rad((float)state.angle_deg);

    // first use sokol-gl to render a rotating cube into the sokol-gl context,
    // this just records draw command for the later sokol-gfx render pass
    sgl_set_context(state.offscreen.sgl_ctx);
    sgl_defaults();
    sgl_load_pipeline(state.offscreen.sgl_pip);
    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(45.0f), 1.0f, 0.1f, 100.0f);
    const float eye[3] = { sinf(a) * 4.0f, sinf(a) * 2.0f, cosf(a) * 4.0f };
    sgl_matrix_mode_modelview();
    sgl_lookat(eye[0], eye[1], eye[2], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    draw_cube();

    // specify the Dear ImGui UI (this also just records commands)
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });

    igSetNextWindowPos((ImVec2){20, 20}, ImGuiCond_Once, (ImVec2){0, 0});
    igSetNextWindowSize((ImVec2){540, 560}, ImGuiCond_Once);
    if (igBegin("Dear ImGui with sokol-gfx images and sampler", 0, 0)) {
        const ImVec4 white = {1, 1, 1, 1};
        const ImVec2 size = {256, 256};
        const ImVec2 uv0 = { 0, 0 };
        const ImVec2 uv1 = { 1, 1 };
        const ImVec2 uv2 = { -1.5f, -1.5f };
        const ImVec2 uv3 = { +2.5f, +2.5f, };
        igImage(simgui_imtextureid(state.ui.img_nearest_clamp), size, uv0, uv1, white, white); igSameLine(0, 4);
        igImage(simgui_imtextureid(state.ui.img_linear_clamp), size, uv0, uv1, white, white);
        igImage(simgui_imtextureid(state.ui.img_nearest_repeat), size, uv2, uv3, white, white); igSameLine(0, 4);
        igImage(simgui_imtextureid(state.ui.img_linear_mirror), size, uv2, uv3, white, white);
    }
    igEnd();

    // perform sokol-gfx rendering...
    // ...first the offscreen pass which renders the sokol-gl scene
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sgl_context_draw(state.offscreen.sgl_ctx);
    sg_end_pass();

    // then the display pass with the Dear ImGui scene
    sg_begin_default_pass(&state.display.pass_action, sapp_width(), sapp_height());
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sgl_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .width = 580,
        .height = 600,
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .window_title = "imgui-images-sapp",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}

// vertex specification for a cube with colored sides and texture coords
static void draw_cube(void) {
    sgl_begin_quads();
    sgl_c3f(1.0f, 0.0f, 0.0f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f, -1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f, -1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(0.0f, 1.0f, 0.0f);
        sgl_v3f_t2f(-1.0f, -1.0f,  1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f,  1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f,  1.0f, -1.0f, -1.0f);
    sgl_c3f(0.0f, 0.0f, 1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f,  1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.5f, 0.0f);
        sgl_v3f_t2f(1.0f, -1.0f,  1.0f, -1.0f,   1.0f);
        sgl_v3f_t2f(1.0f, -1.0f, -1.0f,  1.0f,   1.0f);
        sgl_v3f_t2f(1.0f,  1.0f, -1.0f,  1.0f,  -1.0f);
        sgl_v3f_t2f(1.0f,  1.0f,  1.0f, -1.0f,  -1.0f);
    sgl_c3f(0.0f, 0.5f, 1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f,  1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.0f, 0.5f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f,  1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f, -1.0f, -1.0f, -1.0f);
    sgl_end();
}
