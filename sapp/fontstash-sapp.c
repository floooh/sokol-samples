//------------------------------------------------------------------------------
//  fontstash-sapp.c
//
//  Text rendering via fontstash, stb_truetype and sokol_fontstash.h
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
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "fontstash/fontstash.h"
#define SOKOL_FONTSTASH_IMPL
#include "sokol_fontstash.h"
#include "dbgui/dbgui.h"

typedef struct {
    FONScontext* fons;
    float dpi_scale;
    int font_normal;
    int font_italic;
    int font_bold;
    int font_japanese;
    uint8_t font_normal_data[256 * 1024];
    uint8_t font_italic_data[256 * 1024];
    uint8_t font_bold_data[256 * 1024];
    uint8_t font_japanese_data[2 * 1024 * 1024];
} state_t;
static state_t state;

/* sokol-fetch load callbacks */
static void font_normal_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_normal = fonsAddFontMem(state.fons, "sans", response->buffer_ptr, response->fetched_size,  false);
    }
}

static void font_italic_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_italic = fonsAddFontMem(state.fons, "sans-italic", response->buffer_ptr, response->fetched_size, false);
    }
}

static void font_bold_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_bold = fonsAddFontMem(state.fons, "sans-bold", response->buffer_ptr, response->fetched_size, false);
    }
}

static void font_japanese_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_japanese = fonsAddFontMem(state.fons, "sans-japanese", response->buffer_ptr, response->fetched_size, false);
    }
}

/* round to next power of 2 (see bit-twiddling-hacks) */
static int round_pow2(float v) {
    uint32_t vi = ((uint32_t) v) - 1;
    for (uint32_t i = 0; i < 5; i++) {
        vi |= (vi >> (1<<i));
    }
    return (int) (vi + 1);
}

static void init(void) {
    state.dpi_scale = sapp_dpi_scale();
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());
    sgl_setup(&(sgl_desc_t){0});

    /* make sure the fontstash atlas width/height is pow-2 */
    const int atlas_dim = round_pow2(512.0f * state.dpi_scale);
    FONScontext* fons_context = sfons_create(atlas_dim, atlas_dim, FONS_ZERO_TOPLEFT);
    state.fons = fons_context;
    state.font_normal = FONS_INVALID;
    state.font_italic = FONS_INVALID;
    state.font_bold = FONS_INVALID;
    state.font_japanese = FONS_INVALID;

    /* use sokol_fetch for loading the TTF font files */
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 1,
        .num_lanes = 4
    });
    sfetch_send(&(sfetch_request_t){
        .path = "DroidSerif-Regular.ttf",
        .callback = font_normal_loaded,
        .buffer_ptr = state.font_normal_data,
        .buffer_size = sizeof(state.font_normal_data)
    });
    sfetch_send(&(sfetch_request_t){
        .path = "DroidSerif-Italic.ttf",
        .callback = font_italic_loaded,
        .buffer_ptr = state.font_italic_data,
        .buffer_size = sizeof(state.font_italic_data)
    });
    sfetch_send(&(sfetch_request_t){
        .path = "DroidSerif-Bold.ttf",
        .callback = font_bold_loaded,
        .buffer_ptr = state.font_bold_data,
        .buffer_size = sizeof(state.font_bold_data)
    });
    sfetch_send(&(sfetch_request_t){
        .path = "DroidSansJapanese.ttf",
        .callback = font_japanese_loaded,
        .buffer_ptr = state.font_japanese_data,
        .buffer_size = sizeof(state.font_japanese_data)
    });
}

static void line(float sx, float sy, float ex, float ey)
{
    sgl_begin_lines();
    sgl_c4b(255, 255, 0, 128);
    sgl_v2f(sx, sy);
    sgl_v2f(ex, ey);
    sgl_end();
}

static void frame(void) {
    const float dpis = state.dpi_scale;

    /* pump sokol_fetch message queues */
    sfetch_dowork();

    /* text rendering via fontstash.h */
    float sx, sy, dx, dy, lh = 0.0f;
    uint32_t white = sfons_rgba(255, 255, 255, 255);
    uint32_t black = sfons_rgba(0, 0, 0, 255);
    uint32_t brown = sfons_rgba(192, 128, 0, 128);
    uint32_t blue  = sfons_rgba(0, 192, 255, 255);
    fonsClearState(state.fons);

    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)sapp_width(), (float)sapp_height(), 0.0f, -1.0f, +1.0f);

    sx = 50*dpis; sy = 50*dpis;
    dx = sx; dy = sy;

    FONScontext* fs = state.fons;
    if (state.font_normal != FONS_INVALID) {
        fonsSetFont(fs, state.font_normal);
        fonsSetSize(fs, 124.0f*dpis);
        fonsVertMetrics(fs, NULL, NULL, &lh);
        dx = sx;
        dy += lh;
        fonsSetColor(fs, white);
        dx = fonsDrawText(fs, dx, dy, "The quick ", NULL);
    }
    if (state.font_italic != FONS_INVALID) {
        fonsSetFont(fs, state.font_italic);
        fonsSetSize(fs, 48.0f*dpis);
        fonsSetColor(fs, brown);
        dx = fonsDrawText(fs, dx, dy, "brown ", NULL);
    }
    if (state.font_normal != FONS_INVALID) {
        fonsSetFont(fs, state.font_normal);
        fonsSetSize(fs, 24.0f*dpis);
        fonsSetColor(fs, white);
        dx = fonsDrawText(fs, dx, dy,"fox ", NULL);
    }
    if ((state.font_normal != FONS_INVALID) && (state.font_italic != FONS_INVALID) && (state.font_bold != FONS_INVALID)) {
        fonsVertMetrics(fs, NULL, NULL, &lh);
        dx = sx;
        dy += lh*1.2f;
        fonsSetFont(fs, state.font_italic);
        dx = fonsDrawText(fs, dx, dy, "jumps over ",NULL);
        fonsSetFont(fs, state.font_bold);
        dx = fonsDrawText(fs, dx, dy, "the lazy ",NULL);
        fonsSetFont(fs, state.font_normal);
        dx = fonsDrawText(fs, dx, dy, "dog.",NULL);
    }
    if (state.font_normal != FONS_INVALID) {
        dx = sx;
        dy += lh*1.2f;
        fonsSetSize(fs, 12.0f*dpis);
        fonsSetFont(fs, state.font_normal);
        fonsSetColor(fs, blue);
        fonsDrawText(fs, dx,dy,"Now is the time for all good men to come to the aid of the party.",NULL);
    }
    if (state.font_italic != FONS_INVALID) {
        fonsVertMetrics(fs, NULL, NULL, &lh);
        dx = sx;
        dy += lh*1.2f*2;
        fonsSetSize(fs, 18.0f*dpis);
        fonsSetFont(fs, state.font_italic);
        fonsSetColor(fs, white);
        fonsDrawText(fs, dx, dy, "Ég get etið gler án þess að meiða mig.", NULL);
    }
    if (state.font_japanese != FONS_INVALID) {
        fonsVertMetrics(fs, NULL,NULL,&lh);
        dx = sx;
        dy += lh*1.2f;
        fonsSetFont(fs, state.font_japanese);
        fonsDrawText(fs, dx,dy,"私はガラスを食べられます。それは私を傷つけません。",NULL);
    }

    /* Font alignment */
    if (state.font_normal != FONS_INVALID) {
        fonsSetSize(fs, 18.0f*dpis);
        fonsSetFont(fs, state.font_normal);
        fonsSetColor(fs, white);
        dx = 50*dpis; dy = 350*dpis;
        line(dx-10*dpis,dy,dx+250*dpis,dy);
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        dx = fonsDrawText(fs, dx,dy,"Top",NULL);
        dx += 10*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_MIDDLE);
        dx = fonsDrawText(fs, dx,dy,"Middle",NULL);
        dx += 10*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
        dx = fonsDrawText(fs, dx,dy,"Baseline",NULL);
        dx += 10*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BOTTOM);
        fonsDrawText(fs, dx,dy,"Bottom",NULL);
        dx = 150*dpis; dy = 400*dpis;
        line(dx,dy-30*dpis,dx,dy+80.0f*dpis);
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
        fonsDrawText(fs, dx,dy,"Left",NULL);
        dy += 30*dpis;
        fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_BASELINE);
        fonsDrawText(fs, dx,dy,"Center",NULL);
        dy += 30*dpis;
        fonsSetAlign(fs, FONS_ALIGN_RIGHT | FONS_ALIGN_BASELINE);
        fonsDrawText(fs, dx,dy,"Right",NULL);
    }

    /* Blur */
    if (state.font_italic != FONS_INVALID) {
        dx = 500*dpis; dy = 350*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
        fonsSetSize(fs, 60.0f*dpis);
        fonsSetFont(fs, state.font_italic);
        fonsSetColor(fs, white);
        fonsSetSpacing(fs, 5.0f*dpis);
        fonsSetBlur(fs, 10.0f);
        fonsDrawText(fs, dx,dy,"Blurry...",NULL);
    }

    if (state.font_bold != FONS_INVALID) {
        dy += 50.0f*dpis;
        fonsSetSize(fs, 18.0f*dpis);
        fonsSetFont(fs, state.font_bold);
        fonsSetColor(fs, black);
        fonsSetSpacing(fs, 0.0f);
        fonsSetBlur(fs, 3.0f);
        fonsDrawText(fs, dx,dy+2,"DROP THAT SHADOW",NULL);
        fonsSetColor(fs, white);
        fonsSetBlur(fs, 0);
        fonsDrawText(fs, dx,dy,"DROP THAT SHADOW",NULL);
    }

    /* flush fontstash's font atlas to sokol-gfx texture */
    sfons_flush(fs);

    /* render pass */
    sg_begin_default_pass(&(sg_pass_action){
        .colors[0] = {
            .action = SG_ACTION_CLEAR, .val = { 0.3f, 0.3f, 0.32f, 1.0f }
        }
    }, sapp_width(), sapp_height());
    sgl_draw();
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
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .high_dpi = true,
        .gl_force_gles2 = true,
        .window_title = "fontstash"
    };
}
