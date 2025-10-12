//------------------------------------------------------------------------------
//  nuklear-images-sapp.c
//
//  Demonstrate/test using sg_image textures via nk_image in Nuklear UI.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"

#define SOKOL_GL_IMPL
#include "sokol_gl.h"

// include nuklear.h before the sokol_nuklear.h implementation
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS
#include "nuklear/nuklear.h"
#define SOKOL_NUKLEAR_IMPL
#include "sokol_nuklear.h"

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
        sg_view color_tex_view;
        sg_view color_att_view;
        sg_image depth_img;
        sg_view depth_att_view;
        sg_pass pass;
    } offscreen;
    struct {
        sg_pass_action pass_action;
    } display;
    struct {
        snk_image_t img_nearest_clamp;
        snk_image_t img_linear_clamp;
        snk_image_t img_nearest_repeat;
        snk_image_t img_linear_mirror;
    } ui;
} state;

static void draw_cube(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());
    snk_setup(&(snk_desc_t){
        .enable_set_mouse_cursor = true,
        .dpi_scale = sapp_dpi_scale(),
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

    // color and depth render target images and views for the offscreen pass
    state.offscreen.color_img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "color-image",
    });
    state.offscreen.color_tex_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = state.offscreen.color_img },
        .label = "color-tex-view",
    });
    state.offscreen.color_att_view = sg_make_view(&(sg_view_desc){
        .color_attachment = { .image = state.offscreen.color_img },
        .label = "color-att-view",
    });
    state.offscreen.depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_DEPTH_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "depth-image",
    });
    state.offscreen.depth_att_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment = { .image = state.offscreen.depth_img },
        .label = "depth-att-view",
    });

    // a pass struct for the offscreen render pass
    state.offscreen.pass = (sg_pass){
        // clear to black
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0, 0, 0, 1 },
        },
        // color- and depth-attachemnt-views
        .attachments = {
            .colors[0] = state.offscreen.color_att_view,
            .depth_stencil = state.offscreen.depth_att_view,
        },
    };

    // a pass action for the default pass which clears to blue-ish
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f } },
    };

    // sokol-nuklear image-sampler-pair wrappers which combine the offscreen
    // render target texture with different sampler types
    state.ui.img_nearest_clamp = snk_make_image(&(snk_image_desc_t){
        .texture_view = state.offscreen.color_tex_view,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        })
    });
    state.ui.img_linear_clamp = snk_make_image(&(snk_image_desc_t){
        .texture_view = state.offscreen.color_tex_view,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        })
    });
    state.ui.img_nearest_repeat = snk_make_image(&(snk_image_desc_t){
        .texture_view = state.offscreen.color_tex_view,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_REPEAT,
        })
    });
    state.ui.img_linear_mirror = snk_make_image(&(snk_image_desc_t){
        .texture_view = state.offscreen.color_tex_view,
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

    // specific the Nuklear UI (this also just records draw commands which
    // are then rendered later in the frame in the sokol-gfx default pass)
    struct nk_context* ctx = snk_new_frame();
    nk_style_hide_cursor(ctx);
    if (nk_begin(ctx, "Sokol + Nuklear Image Test", nk_rect(10, 10, 540, 570), NK_WINDOW_BORDER|NK_WINDOW_SCALABLE|NK_WINDOW_MOVABLE|NK_WINDOW_MINIMIZABLE)) {
        nk_layout_row_static(ctx, 256, 256, 2);
        const struct nk_rect region = { 0, 0, 4, 4 };
        nk_image(ctx, nk_image_handle(snk_nkhandle(state.ui.img_nearest_clamp)));
        nk_image(ctx, nk_image_handle(snk_nkhandle(state.ui.img_linear_clamp)));
        nk_image(ctx, nk_subimage_handle(snk_nkhandle(state.ui.img_nearest_repeat), 1, 1, region));
        nk_image(ctx, nk_subimage_handle(snk_nkhandle(state.ui.img_linear_mirror), 1, 1, region));
    }
    nk_end(ctx);

    // perform sokol-gfx rendering...
    // ...first the offscreen pass which renders the sokol-gl scene
    sg_begin_pass(&state.offscreen.pass);
    sgl_context_draw(state.offscreen.sgl_ctx);
    sg_end_pass();

    // then the display pass with the Dear ImGui scene
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain_next() });
    snk_render(sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (__dbgui_event_with_retval(ev)) {
        return;
    }
    snk_handle_event(ev);
}

static void cleanup(void) {
    sgl_shutdown();
    snk_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .enable_clipboard = true,
        .width = 580,
        .height = 600,
        .window_title = "nuklear-image-sapp.c",
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
