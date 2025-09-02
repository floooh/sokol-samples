//------------------------------------------------------------------------------
//  basisu-sapp.c
//
//  This is mainly a regression test for compressed texture format support
//  in sokol_gfx.h, but it's also a simple demo of how to use Basis Universal
//  textures. Basis Univsersal compressed textures are embedded as C arrays
//  so that texture data doesn't need to be loaded (for instance via sokol_fetch.h)
//
//  Texture credits: Paul Vera-Broadbent (twitter: @PVBroadz)
//
//  And some useful info from Carl Woffenden (twitter: @monsieurwoof):
//
//  "The testcard image, BTW, was specifically crafted to compress well with
//  ETC1S. The regions fit into 4x4 bounds, with flat areas having only two
//  colours (and gradients designed to work across the endpoints in a block).
//  @PVBroadz created it when we were experimenting with BasisU."
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"
#include "data/basisu-assets.h"
#include "basisu/sokol_basisu.h"

static struct {
    sg_pass_action pass_action;
    sgl_pipeline alpha_pip;
    sg_view opaque_view;
    sg_view alpha_view;
    sg_sampler smp;
    double angle_deg;
} state = {
    .pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.25f, 0.25f, 1.0f, 1.0f }}
    }
};

// NOTE: only the pixel formats used in the basisu wrapper code supported here
static const char* pixelformat_to_str(sg_pixel_format fmt) {
    switch (fmt) {
        case SG_PIXELFORMAT_BC3_RGBA:       return "BC3 RGBA";
        case SG_PIXELFORMAT_BC1_RGBA:       return "BC1 RGBA";
        case SG_PIXELFORMAT_ETC2_RGBA8:     return "ETC2 RGBA8";
        case SG_PIXELFORMAT_ETC2_RGB8:      return "ETC2 RGB8";
        default:                            return "???";
    }
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric(),
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func
    });

    // setup Basis Universal via our own minimal wrapper code
    sbasisu_setup();

    // create sokol-gfx textures from the embedded Basis Universal textures
    state.opaque_view = sg_make_view(&(sg_view_desc){
        .texture.image = sbasisu_make_image(SG_RANGE(embed_testcard_basis)),
    });
    state.alpha_view = sg_make_view(&(sg_view_desc){
        .texture.image = sbasisu_make_image(SG_RANGE(embed_testcard_rgba_basis)),
    });

    // create a sampler object
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_LINEAR,
        .max_anisotropy = 8,
    });

    // a sokol-gl pipeline object for alpha-blended rendering
    state.alpha_pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0] = {
            .write_mask = SG_COLORMASK_RGB,
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        }
    });
}

typedef struct {
    struct { float x; float y; } pos;
    struct { float x; float y; } scale;
    float rot;
    sg_view view;
    sgl_pipeline pip;
} quad_params_t;

static void draw_quad(quad_params_t params) {
    sgl_texture(params.view, state.smp);
    if (params.pip.id) {
        sgl_load_pipeline(params.pip);
    }
    else {
        sgl_load_default_pipeline();
    }
    sgl_push_matrix();
    sgl_translate(params.pos.x, params.pos.y, 0.0f);
    sgl_scale(params.scale.x, params.scale.y, 0.0f);
    sgl_rotate(params.rot, 0.0f, 0.0f, 1.0f);
    sgl_begin_quads();
    sgl_v2f_t2f(-1.0f, -1.0f, 0.0f, 0.0f);
    sgl_v2f_t2f(+1.0f, -1.0f, 1.0f, 0.0f);
    sgl_v2f_t2f(+1.0f, +1.0f, 1.0f, 1.0f);
    sgl_v2f_t2f(-1.0f, +1.0f, 0.0f, 1.0f);
    sgl_end();
    sgl_pop_matrix();
}

static void frame(void) {
    // info text
    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_origin(0.5f, 2.0f);
    sdtx_printf("Opaque format: %s\n\n", pixelformat_to_str(sbasisu_pixelformat(false)));
    sdtx_printf("Alpha format: %s", pixelformat_to_str(sbasisu_pixelformat(true)));

    // draw some textured quads via sokol-gl
    sgl_defaults();
    sgl_enable_texture();
    sgl_matrix_mode_projection();
    const float aspect = sapp_heightf() / sapp_widthf();
    sgl_ortho(-1.0f, +1.0f, aspect, -aspect, -1.0f, +1.0f);
    sgl_matrix_mode_modelview();
    state.angle_deg += sapp_frame_duration() * 60.0;
    draw_quad((quad_params_t){
        .pos = { -0.425f, 0.0f },
        .scale = { 0.4f, 0.4f },
        .rot = sgl_rad((float)state.angle_deg),
        .view = state.opaque_view,
    });
    draw_quad((quad_params_t){
        .pos = { +0.425f, 0.0f },
        .scale = { 0.4f, 0.4f },
        .rot = -sgl_rad((float)state.angle_deg),
        .view = state.alpha_view,
        .pip = state.alpha_pip,
    });

    // ...and the actual rendering
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sgl_draw();
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sbasisu_shutdown();
    sgl_shutdown();
    sdtx_shutdown();
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
        .width = 800,
        .height = 600,
        .window_title = "basisu-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
