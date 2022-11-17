//------------------------------------------------------------------------------
//  fontstash-layers.c
//  Demomstrates layered rendering with sokol_fontstash.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include <stdio.h>  // needed by fontstash's IO functions even though they are not used
#define FONTSTASH_IMPLEMENTATION
#if defined(_MSC_VER )
#pragma warning(disable:4996)   // strncpy use in fontstash.h
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "fontstash/fontstash.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#define SOKOL_FONTSTASH_IMPL
#include "sokol_fontstash.h"
#include "dbgui/dbgui.h"
#include "util/fileutil.h"
#include "fontstash-layers-sapp.glsl.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    FONScontext* fons;
    float dpi_scale;
    int font;
    uint8_t font_data[256 * 1024];
} state;

// round to next power of 2 (see bit-twiddling-hacks)
static int round_pow2(float v) {
    uint32_t vi = ((uint32_t) v) - 1;
    for (uint32_t i = 0; i < 5; i++) {
        vi |= (vi >> (1<<i));
    }
    return (int) (vi + 1);
}

// sokol-fetch callback for TTF font data
static void font_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font = fonsAddFontMem(state.fons, "sans", (void*)response->data.ptr, (int)response->data.size, false);
    }
}

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());
    sgl_setup(&(sgl_desc_t){0});
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 1,
        .num_lanes = 1,
    });

    // make sure fontstash atlas width/height is pow 2
    const int atlas_dim = round_pow2(512.0f * sapp_dpi_scale());
    state.fons = sfons_create(&(sfons_desc_t){ .width = atlas_dim, .height = atlas_dim });
    state.font = FONS_INVALID;

    // use sokol-fetch to load TTF font file
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("DroidSerif-Regular.ttf", path_buf, sizeof(path_buf)),
        .callback = font_loaded,
        .buffer = SFETCH_RANGE(state.font_data),
    });

    // pass action to clear framebuffer to black
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // vertex buffer, shader and pipeline (with alpha-blending) for a triangle
    float vertices[] = {
        // positions            // colors
         0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 0.9f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 0.9f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 0.9f
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
    });
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(triangle_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_color0].format = SG_VERTEXFORMAT_FLOAT4,
            }
        },
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
    // pump the sokol-fetch message queues
    sfetch_dowork();

    const float dpis = sapp_dpi_scale();
    const float disp_w = sapp_widthf();
    const float disp_h = sapp_heightf();

    // prepare sokol-gl
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, disp_w, disp_h, 0.0f, -1.0f, +1.0f);

    // only render text once font data has been loaded
    FONScontext* fs = state.fons;
    fonsClearState(fs);
    if (state.font != FONS_INVALID) {
        fonsSetFont(fs, state.font);
        fonsSetSize(fs, 124.0f * dpis);
        fonsSetColor(fs, 0xFFFFFFFF);
        float lh;
        fonsVertMetrics(fs, 0, 0, &lh);
        // background text to sokol-gl layer 0
        {
            const char* text = "Background";
            sgl_layer(0);
            const float w = fonsTextBounds(fs, 0, 0, text, 0, 0);
            const float x = (disp_w - w) * 0.5f;
            const float y = (disp_h * 0.5f) - lh * 0.25f;
            fonsDrawText(fs, x, y, text, 0);
        }
        // foreground text to sokol-gl layer 1
        {
            const char* text = "Foreground";
            sgl_layer(1);
            const float w = fonsTextBounds(fs, 0, 0, text, 0, 0);
            const float x = (disp_w - w) * 0.5f;
            const float y = (disp_h * 0.5f) + lh * 1.0f;
            fonsDrawText(fs, x, y, text, 0);
        }
    }
    sfons_flush(fs);

    // sokol-gfx render pass
    sg_begin_default_passf(&state.pass_action, disp_w, disp_h);

    // draw background text layer via sokol-gl
    sgl_draw_layer(0);

    // render a triangle inbetween text layers
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);

    // draw foreground text layer via sokol-gl
    sgl_draw_layer(1);

    __dbgui_draw();
    sg_end_pass();
    sg_commit();

}

static void cleanup(void) {
    __dbgui_shutdown();
    sfetch_shutdown();
    sfons_destroy(state.fons);
    sgl_shutdown();
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
        .high_dpi = true,
        .window_title = "fontstash-layers-sapp.c",
        .icon.sokol_default = true,
    };
}