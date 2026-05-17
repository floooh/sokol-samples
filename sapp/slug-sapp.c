//------------------------------------------------------------------------------
//  slug-sapp.c
//
//  Demonstrates Slug text rendering. Ported from sokol-slug-odin:
//  https://tangled.org/dosha.dev/sokol-slug-odin/
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#define SOKOL_APP_IMGUI_IMPL
#include "sokol_app_imgui.h"
#include "util/fileutil.h"
#include "slugutil/slugutil.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "slug-sapp.glsl.h"

#define MAX_FONTS (3)
#define MAX_TTF_SIZE (2 * 1024 * 1024)
#define MAX_INSTANCES (16 * 1024)
#define MAX_DRAWS (128)
#define TOTAL_LINES (6)
#define LINE_HEIGHT (80.0f)
#define FONT_SIZE (48.0f)

uint32_t line[6][128];

static struct {
    sg_pass_action pass_action;
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_pipeline pip;
    sg_sampler smp;
    struct {
        float zoom;
        float pan_x;
        float pan_y;
        bool dragging;
    } inp;
    struct {
        slug_font_t cairo;
        slug_font_t lucide;
        slug_font_t twemoji;
    } fonts;
    struct {
        int start_instance;
        int cur_instance;
        int cur_draw_cmd;
        const slug_font_t* cur_font;
    } draw;
} state;

typedef struct {
    vec4_t draw_rect;
    vec4_t glyph_bbox;
    vec4_t band_transform;
    int glyph_params[4];
    vec4_t color;
} instance_t;

typedef struct {
    int base_instance;
    int num_instances;
    sg_view curve_tex_view;
    sg_view band_tex_view;
} draw_t;

uint8_t file_buffers[MAX_FONTS][MAX_TTF_SIZE];

instance_t instances[MAX_INSTANCES];
draw_t draws[MAX_DRAWS];

static float measure_line(const slug_font_t* font, const uint32_t* text);
static void begin_text(void);
static void push_centered_line(const slug_font_t* font, const uint32_t* text, int line_nr, const mat44_t* mvp);
static void push_centered_line_emoji(const slug_font_t* font, const uint32_t* text, int line_nr, const mat44_t* mvp);
static void push_line(const slug_font_t* font, const uint32_t* text, float x, float y, const mat44_t* mvp);
static void push_line_emoji(const slug_font_t* font, const uint32_t* text, float x, float y, const mat44_t* mvp);
static void push_emoji(const slug_font_t* font, const uint32_t codepoint, float x, float y, const mat44_t* mvp);
static void push_glyph(const slug_font_t* font, const slug_glyph_t* glyph, float x, float y, const mat44_t* mvp, vec4_t color);
static void push_draw_cmd(void);
static void end_text(void);
static void cairo_fetch_callback(const sfetch_response_t* response);
static void lucide_fetch_callback(const sfetch_response_t* response);
static void twemoji_fetch_callback(const sfetch_response_t* response);
static void draw_ui(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 1,
        .num_lanes = 3,
        .max_requests = 3,
        .logger.func = slog_func,
    });
    sgimgui_setup(&(sgimgui_desc_t){0});
    sappimgui_setup();
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });

    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.1f, 0.1f, 0.1f, 1.0f } },
    };
    state.inp.zoom = 1.0f;

    // populate unicode lines, first the latin characters
    for (uint32_t i = 0; i < 32; i++) {
        line[0][i] = 0x40 + i;
        if (i != 31) {
            line[1][i] = 0x60 + i;
        }
        line[2][i] = 0x20 + i;
    }
    // tha arabic alphabet
    for (uint32_t i = 0, cp = 0x0627; cp <= 0x064A; cp++) {
        if ((cp < 0x063B) || (cp > 0x063F)) {
            line[3][i++] = cp;
        }
    }
    // icons from lucide font, note that the unicode assignment
    // of icons is very messy, it has gaps and duplicates. The
    // range picked here is somewhat 'orderly'.
    for (uint32_t i = 0, cp = 0xE29A; cp <= 0xE2BA; cp++) {
        line[4][i++] = cp;
    }
    // emojis
    line[5][0] = 0x1F600;   // grinning face
    line[5][1] = 0x1F60D;   // heart eyes
    line[5][2] = 0x1F60E;   // sunglasses
    line[5][3] = 0x1F525;   // fire
    line[5][4] = 0x1F44D;   // thumbs up
    line[5][5] = 0x1F389;   // party popper
    line[5][6] = 0x1F680;   // rocket
    line[5][7] = 0x2764;    // red heart
    line[5][8] = 0x1F308;   // rainbow
    line[5][9] = 0x1F31F;   // glowing star
    line[5][10] = 0x1F3B5;  // musical note
    line[5][11] = 0x1F40D;  // snake
    line[5][12] = 0x1F436;  // dog face
    line[5][13] = 0x1F431;  // cat face
    line[5][14] = 0x1F34E;  // red apple
    line[5][15] = 0x1F370;  // shortcake

    // buffers, shader, pipeline, sampler
    const float quad_verts[] = {
        0.0f, 0.0f, // bottom-left
        1.0f, 0.0f, // bottom-right
        0.0f, 1.0f, // top-left
        1.0f, 1.0f, // top-right
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_verts),
        .label = "slug-quad-vbuf",
    });
    const uint16_t quad_indices[] = { 0, 1, 2, 2, 1, 3 };
    state.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(quad_indices),
        .label = "slug-quad-ibuf",
    });
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(slug_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_slug_quad_pos].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .colors[0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_ONE,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            }
        },
        .label = "slug-pipeline"
    });
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "slug-sampler",
    });

    // start loading fonts
    char buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("Cairo.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[0], .size = sizeof(file_buffers[0]) },
        .callback = cairo_fetch_callback,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("lucide.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[1], .size = sizeof(file_buffers[1]) },
        .callback = lucide_fetch_callback,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("twemoji.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[2], .size = sizeof(file_buffers[2]) },
        .callback = twemoji_fetch_callback,
    });
}

static void frame(void) {
    sfetch_dowork();
    draw_ui();

    // FIXME: replace with mat44_ortho func
    float sx = 2.0f / ((sapp_widthf() / state.inp.zoom));
    float sy = 2.0f / ((sapp_heightf() / state.inp.zoom));
    float tx = -1.0f - state.inp.pan_x * sx;
    float ty = -1.0f - state.inp.pan_y * sy;
    mat44_t mvp = {
        .x = vec4(sx, 0.0f, 0.0f, 0.0f),
        .y = vec4(0.0f, sy, 0.0f, 0.0f),
        .z = vec4(0.0f, 0.0f, -1.0f, 0.0f),
        .w = vec4(tx, ty, 0.0f, 1.0f),
    };

    begin_text();

    bool any_valid = state.fonts.cairo.valid || state.fonts.lucide.valid || state.fonts.twemoji.valid;
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    if (any_valid) {
        sg_apply_pipeline(state.pip);
    }
    // latin and arabic codepoints
    if (state.fonts.cairo.valid) {
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.vbuf,
            .index_buffer = state.ibuf,
            .views = {
                [VIEW_band_tex] = state.fonts.cairo.band.tex_view,
                [VIEW_curve_tex] = state.fonts.cairo.curve.tex_view,
            },
            .samplers[SMP_point_sampler] = state.smp,
        });
        for (int i = 0; i < 4; i++) {
            push_centered_line(&state.fonts.cairo, line[i], i, &mvp);
        }
    }
    if (state.fonts.lucide.valid) {
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.vbuf,
            .index_buffer = state.ibuf,
            .views = {
                [VIEW_band_tex] = state.fonts.lucide.band.tex_view,
                [VIEW_curve_tex] = state.fonts.lucide.curve.tex_view,
            },
            .samplers[SMP_point_sampler] = state.smp,
        });
        push_centered_line(&state.fonts.lucide, line[4], 4, &mvp);
    }
    if (state.fonts.twemoji.valid) {
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.vbuf,
            .index_buffer = state.ibuf,
            .views = {
                [VIEW_band_tex] = state.fonts.twemoji.band.tex_view,
                [VIEW_curve_tex] = state.fonts.twemoji.curve.tex_view,
            },
            .samplers[SMP_point_sampler] = state.smp,
        });
        push_centered_line_emoji(&state.fonts.twemoji, line[5], 5, &mvp);
    }
    end_text(); // FIXME FIXME FIXME
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    sappimgui_track_event(ev);
    if (simgui_handle_event(ev)) {
        return;
    }
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            {
                float h = sapp_heightf();
                float mouse_world_x = state.inp.pan_x + ev->mouse_x / state.inp.zoom;
                float mouse_world_y = state.inp.pan_y + (h - ev->mouse_y) / state.inp.zoom;
                state.inp.zoom *= 1.0 + ev->scroll_y * 0.1f;
                if (state.inp.zoom < 0.1f) {
                    state.inp.zoom = 0.1f;
                } else if (state.inp.zoom > 50.0f) {
                    state.inp.zoom = 50.0f;
                }
                // Adjust pan so the world point under the mouse stays fixed
                state.inp.pan_x = mouse_world_x - ev->mouse_x / state.inp.zoom;
                state.inp.pan_y = mouse_world_y - (h - ev->mouse_y) / state.inp.zoom;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                state.inp.dragging = true;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                state.inp.dragging = false;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_ENTER:
        case SAPP_EVENTTYPE_UNFOCUSED:
        case SAPP_EVENTTYPE_SUSPENDED:
        case SAPP_EVENTTYPE_ICONIFIED:
            state.inp.dragging = false;
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            if (state.inp.dragging) {
                state.inp.pan_x -= ev->mouse_dx / state.inp.zoom;
                state.inp.pan_y += ev->mouse_dy / state.inp.zoom;
            }
            break;
        default: break;
    }
}

static void cleanup(void) {
    slug_unload_font(&state.fonts.cairo);
    slug_unload_font(&state.fonts.lucide);
    slug_unload_font(&state.fonts.twemoji);
    sfetch_shutdown();
    sappimgui_shutdown();
    sgimgui_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

static void draw_ui(void) {
    sappimgui_track_frame();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        sappimgui_draw_menu("sokol-app");
        igEndMainMenuBar();
    }
    sgimgui_draw();
    sappimgui_draw();
}

static void cairo_fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.cairo, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static void lucide_fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.lucide, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static void twemoji_fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.twemoji, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static float measure_line(const slug_font_t* font, const uint32_t* text) {
    float total = 0.0f;
    uint32_t ucp;
    while ((ucp = *text++) != 0) {
        const slug_glyph_t* glyph = slug_get_glyph(font, ucp);
        if (glyph) {
            total += glyph->advance * FONT_SIZE;
        }
    }
    return total;
}

static void begin_text(void) {
    state.draw.start_instance = 0;
    state.draw.cur_instance = 0;
    state.draw.cur_draw_cmd = 0;
    state.draw.cur_font = 0;
}

static void end_text(void) {
    // push final draw command
    push_draw_cmd();
}

static void push_draw_cmd(void) {
    if ((state.draw.cur_draw_cmd < MAX_DRAWS) && (state.draw.cur_instance > state.draw.start_instance)) {
        assert(state.draw.cur_font);
        draws[state.draw.cur_draw_cmd++] = (draw_t){
            .base_instance = state.draw.start_instance,
            .num_instances = state.draw.cur_instance - state.draw.start_instance,
            .curve_tex_view = state.draw.cur_font->curve.tex_view,
            .band_tex_view = state.draw.cur_font->band.tex_view,
        };
        state.draw.start_instance = state.draw.cur_instance;
    }
}

static void push_instance(const instance_t* instance) {
    if (state.draw.cur_instance < MAX_INSTANCES) {
        instances[state.draw.cur_instance++] = *instance;
    }
}

static void push_centered_line(const slug_font_t* font, const uint32_t* text, int line_nr, const mat44_t* mvp) {
    const float line_height = LINE_HEIGHT;
    const int total_lines = TOTAL_LINES;
    const float block_height = (float)total_lines * line_height;
    float line_width = measure_line(font, text);
    float base_x = (sapp_widthf() - line_width) * 0.5f;
    float base_y = (sapp_heightf() + block_height) * 0.5f - (float)line_nr * line_height;
    push_line(font, text, base_x, base_y, mvp);
}

static void push_centered_line_emoji(const slug_font_t* font, const uint32_t* text, int line_nr, const mat44_t* mvp) {
    const float line_height = LINE_HEIGHT;
    const int total_lines = TOTAL_LINES;
    const float block_height = (float)total_lines * line_height;
    float line_width = measure_line(font, text);
    float base_x = (sapp_widthf() - line_width) * 0.5f;
    float base_y = (sapp_heightf() + block_height) * 0.5f - (float)line_nr * line_height;
    push_line_emoji(font, text, base_x, base_y, mvp);
}

static void push_line(const slug_font_t* font, const uint32_t* text, float x, float y, const mat44_t* mvp) {
    uint32_t cp = 0;
    while ((cp = *text++) != 0) {
        const slug_glyph_t* glyph = slug_get_glyph(font, cp);
        if (glyph) {
            push_glyph(font, glyph, x, y, mvp, vec4(1.0f, 1.0f, 1.0f, 1.0f));
            x += glyph->advance * FONT_SIZE;
        }
    }
}

static void push_line_emoji(const slug_font_t* font, const uint32_t* text, float x, float y, const mat44_t* mvp) {
    uint32_t cp = 0;
    while ((cp = *text++) != 0) {
        const slug_glyph_t* glyph = slug_get_glyph(font, cp);
        if (glyph) {
            push_emoji(font, cp, x, y, mvp);
            x += glyph->advance * FONT_SIZE;
        }
    }
}

static void push_emoji(const slug_font_t* font, uint32_t codepoint, float x, float y, const mat44_t* mvp) {
    const slug_colr_base_t* colr_base = slug_find_colr_base(font, codepoint);
    if (colr_base == 0) {
        return;
    }
    // draw each layer as its own glyph
    for (uint16_t i = 0; i < colr_base->num_layers; i++) {
        slug_colr_layer_t* layer = &font->colr_layers[colr_base->first_layer + i];
        if ((layer->glyph_id < 0) || (layer->glyph_id >= arrlen(font->glyphs))) {
            continue;
        }
        slug_glyph_t* glyph = &font->glyphs[layer->glyph_id];
        vec4_t color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        if (layer->palette_index < arrlen(font->cpal_colors)) {
            color = font->cpal_colors[layer->palette_index];
        }
        push_glyph(font, glyph, x, y, mvp, color);
    }
}

static void push_glyph(const slug_font_t* font, const slug_glyph_t* glyph, float x, float y, const mat44_t* mvp, vec4_t color) {
    if ((glyph->max_band_x < 0.0f) || (glyph->max_band_y < 0.0f)) {
        return;
    }
    if (font != state.draw.cur_font) {
        if (state.draw.cur_font != 0) {
            push_draw_cmd();
        }
        state.draw.cur_font = font;
    }
    const instance_t glyph_instance = {
        .draw_rect = {
            x + (glyph->bbox.x0 * FONT_SIZE),
            y + (glyph->bbox.y0 * FONT_SIZE),
            (glyph->bbox.x1 - glyph->bbox.x0) * FONT_SIZE,
            (glyph->bbox.y1 - glyph->bbox.y0) * FONT_SIZE,
        },
        .glyph_bbox = {
            glyph->bbox.x0,
            glyph->bbox.y0,
            glyph->bbox.x1,
            glyph->bbox.y1,
        },
        .band_transform = {
            glyph->band_scale.x,
            glyph->band_scale.y,
            glyph->band_offset.x,
            glyph->band_offset.y,
        },
        .glyph_params = {
            glyph->glyph_loc[0],
            glyph->glyph_loc[1],
            (int)glyph->max_band_x,
            (int)glyph->max_band_y,
        },
        .color = color,
    };
    push_instance(&glyph_instance);

    const vs_params_t vs_params = {
        .mvp = *mvp,
        .draw_rect = glyph_instance.draw_rect,
        .glyph_bbox = glyph_instance.glyph_bbox,
    };
    const fs_params_t fs_params = {
        .text_color = color,
        .band_transform = glyph_instance.band_transform,
        .glyph_params = {
            [0] = glyph_instance.glyph_params[0],
            [1] = glyph_instance.glyph_params[1],
            [2] = glyph_instance.glyph_params[2],
            [3] = glyph_instance.glyph_params[3],
        },
    };
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(fs_params));
    sg_draw(0, 6, 1);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .width = 900,
        .height = 500,
        .sample_count = 1,
        .high_dpi = true,
        .window_title = "slug-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
