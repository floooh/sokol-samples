//------------------------------------------------------------------------------
//  compressed-textures-sapp.c
//  Test compressed texture usage and features in sokol-gfx. Uses embedded
//  Basis-Universal texture data which is then transcoded into texture
//  formats supported by the runtime platform.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"
#include "data/compressed-textures-assets.h"
#include "basisu/sokol_basisu.h"

static struct {
    sg_pass_action pass_action;
    sgl_pipeline alpha_pip;
    struct {
        sg_image_desc desc;
        sg_image img;
    } baboon;
    struct {
        sg_image_desc desc;
        sg_image img;
    } testcard;
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.25f, 1.0f, 1.0f }}
    }
};

// NOTE: only the pixel formats used in the basisu wrapper code supported here
static const char* pixelformat_to_str(sg_pixel_format fmt) {
    switch (fmt) {
        case SG_PIXELFORMAT_BC3_RGBA:       return "BC3 RGBA";
        case SG_PIXELFORMAT_BC1_RGBA:       return "BC1 RGBA";
        case SG_PIXELFORMAT_PVRTC_RGB_4BPP: return "PVRTC RGB 4BPP";
        case SG_PIXELFORMAT_ETC2_RGBA8:     return "ETC2 RGBA8";
        case SG_PIXELFORMAT_ETC2_RGB8:      return "ETC2 RGB8";
        default:                            return "???";
    }
}

void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric()
    });
    sgl_setup(&(sgl_desc_t){0});

    // setup Basis Universal
    sbasisu_setup();

    // transcode the embedded basisu images, keep the decoded data around
    // until application shutdown, because we'll need the data in the
    // frame callback for the dynamically updated textures
    state.baboon.desc = sbasisu_transcode(embed_baboon_basis, sizeof(embed_baboon_basis));
    state.testcard.desc = sbasisu_transcode(embed_testcard_rgba_basis, sizeof(embed_testcard_rgba_basis));

    // create textures from the transcoded image data
    if (_SG_PIXELFORMAT_DEFAULT != state.baboon.desc.pixel_format) {
        state.baboon.img = sg_make_image(&state.baboon.desc);
    }
    if (_SG_PIXELFORMAT_DEFAULT != state.testcard.desc.pixel_format) {
        state.testcard.img = sg_make_image(&state.testcard.desc);
    }

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
    sg_image img;
    sgl_pipeline pip;
} quad_params_t;

static void draw_quad(quad_params_t params) {
    sgl_texture(params.img);
    if (params.pip.id) {
        sgl_load_pipeline(params.pip);
    }
    else {
        sgl_load_default_pipeline();
    }
    sgl_push_matrix();
    sgl_translate(params.pos.x, params.pos.y, 0.0f);
    sgl_scale(params.scale.x, params.scale.y, 0.0f);
    sgl_begin_quads();
    sgl_v2f_t2f(-1.0f, -1.0f, 0.0f, 0.0f);
    sgl_v2f_t2f(+1.0f, -1.0f, 1.0f, 0.0f);
    sgl_v2f_t2f(+1.0f, +1.0f, 1.0f, 1.0f);
    sgl_v2f_t2f(-1.0f, +1.0f, 0.0f, 1.0f);
    sgl_end();
    sgl_pop_matrix();
}

void frame(void) {
    // info text
    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_origin(0.5f, 2.0f);
    sdtx_printf("Baboon format:   %s\n\n", pixelformat_to_str(state.baboon.desc.pixel_format));
    sdtx_printf("Testcard format: %s", pixelformat_to_str(state.testcard.desc.pixel_format));

    // draw some textured quads via sokol-gl
    sgl_defaults();
    sgl_enable_texture();
    sgl_matrix_mode_projection();
    const float aspect = sapp_heightf() / sapp_widthf();
    sgl_ortho(-1.0f, +1.0f, aspect, -aspect, -1.0f, +1.0f);
    sgl_matrix_mode_modelview();
    sgl_translate(0.0f, 0.1f, 0.0f);
    draw_quad((quad_params_t){
        .pos = { -0.3f, -0.3f },
        .scale = { 0.25, 0.25f },
        .img = state.baboon.img,
    });
    draw_quad((quad_params_t){
        .pos = { +0.3f, -0.3f },
        .scale = { 0.25f, 0.25f },
        .img = state.testcard.img,
        .pip = state.alpha_pip,
    });
    draw_quad((quad_params_t){
        .pos = { .y=+0.3f },
        .scale = { 0.25f, 0.25f },
        .img = state.baboon.img,
        .pip = state.alpha_pip,
    });

    // ...and the actual rendering
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sgl_draw();
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sbasisu_free(&state.baboon.desc);
    sbasisu_free(&state.testcard.desc);
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
        .sample_count = 4,
        .window_title = "Compressed Textures",
        .icon.sokol_default = true
    };
}
