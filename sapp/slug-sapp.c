//------------------------------------------------------------------------------
//  slug-sapp.c
//
//  Demonstrates Eric Lyengel's Slug text rendering algorithm.
//  Ported from sokol-slug-odin: https://tangled.org/dosha.dev/sokol-slug-odin/
//
//  Also see: https://terathon.com/blog/decade-slug.html
//
//  Main differences from above demo:
//  - 4x reduced band texture size (RGBA32UI => RG16UI)
//  - batched glyph rendering via hardware-instancing instead of one draw call
//    per glyph where each glyph is a triangle-strip, with the corner vertices
//    synthesized in the vertex shader
//  - the pixel size multiplier isn't baked into the preprocessed
//    Slug font data
//  - add `@image_sample_type` and `@sampler_type` annotations to
//    shader to silence validation layer warnings (and allow the sample to
//    work with the WebGPU backend)
//
//  Knows issues:
//  - currently no kerning
//  - TTF-to-Slug data preprocessing should be moved into an offline tool
//  - more optimizations would be possible when switching to storage buffers
//    (both for the curve+band data and the glyph 'vertices')
//  - shader seems to be the Slug 'v1' shader, not the most recent one which
//    has improvements in the vertex shader(?)
//  - in general, also check against the BGFX slug sample, this does a couple
//    of things differently: https://github.com/bkaradzic/bgfx/tree/master/examples/51-gpufont
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
#define MAX_TTF_FILE_SIZE (2 * 1024 * 1024)
#define MAX_DRAWN_GLYPHS (16 * 1024)
#define MAX_DRAW_COMMANDS (128)
#define TOTAL_LINES (6)
#define FONT_SIZE (48.0f)
#define MIN_ZOOM (0.1f)
#define MAX_ZOOM (50.0f)

uint32_t line[6][128];

static struct {
    sg_pass_action pass_action;
    sg_buffer buf;
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
        int start_glyph_vertex;
        int cur_glyph_vertex;
        int cur_draw_command;
        const slug_font_t* cur_font;
    } draw;
    float font_size;
} state;

// per-glyph data in glyph buffer, expanded 4x via hardware-instancing
typedef struct {
    vec4_t draw_rect;
    vec4_t glyph_bbox;
    vec4_t band_transform;
    int16_t glyph_params[4];
    uint32_t color;
} glyph_vertex_t;

typedef struct {
    int base_instance;
    int num_instances;
    sg_view curve_tex_view;
    sg_view band_tex_view;
} draw_command_t;

uint8_t file_buffers[MAX_FONTS][MAX_TTF_FILE_SIZE];

glyph_vertex_t glyph_vertices[MAX_DRAWN_GLYPHS];
draw_command_t draw_commands[MAX_DRAW_COMMANDS];

static float measure_line(const slug_font_t* font, const uint32_t* text);
static void begin_push_glyphs(void);
static void push_centered_line(const slug_font_t* font, const uint32_t* text, int line_nr);
static void push_centered_line_emoji(const slug_font_t* font, const uint32_t* text, int line_nr);
static void push_line(const slug_font_t* font, const uint32_t* text, float x, float y);
static void push_line_emoji(const slug_font_t* font, const uint32_t* text, float x, float y);
static void push_emoji(const slug_font_t* font, const uint32_t codepoint, float x, float y);
static void push_glyph(const slug_font_t* font, const slug_glyph_t* glyph, float x, float y, vec4_t color);
static void push_draw_command(void);
static void end_push_glyphs(void);
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
    state.font_size = FONT_SIZE;

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

    // A stream-update buffer which holds one glyph_vertex_t per glyph, this
    // is expanded 4x via hardware instancing with the 4 corner vertex positions
    // synthesized in the vertex shader.
    // Another option (probably more efficient) might be to place this data
    // in a storage buffer.
    state.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.stream_update = true,
        .size = MAX_DRAWN_GLYPHS * sizeof(glyph_vertex_t),
        .label = "slug-glyph-buffer",
    });

    // the pipeline is configured with a single instance-stepped buffer which
    // provides the per-glyph data, also note that rendering is non-indexed
    // and each glyph is rendered as a 4-vertex triangle-strip
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(slug_shader_desc(sg_query_backend())),
        .layout = {
            .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [ATTR_slug_draw_rect] = { .format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0 },
                [ATTR_slug_glyph_bbox] = { .format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0 },
                [ATTR_slug_in_band_transform] = { .format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0 },
                [ATTR_slug_in_glyph_params] = { .format = SG_VERTEXFORMAT_SHORT4, .buffer_index = 0 },
                [ATTR_slug_in_text_color] = { .format = SG_VERTEXFORMAT_UBYTE4N, .buffer_index = 0 },
            },
        },
        .index_type = SG_INDEXTYPE_NONE,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
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

    float sx = 2.0f / ((sapp_widthf() / state.inp.zoom));
    float sy = 2.0f / ((sapp_heightf() / state.inp.zoom));
    float tx = -1.0f - state.inp.pan_x * sx;
    float ty = -1.0f - state.inp.pan_y * sy;
    const vs_params_t vs_params = {
        .mvp = (mat44_t){
            .x = vec4(sx, 0.0f, 0.0f, 0.0f),
            .y = vec4(0.0f, sy, 0.0f, 0.0f),
            .z = vec4(0.0f, 0.0f, -1.0f, 0.0f),
            .w = vec4(tx, ty, 0.0f, 1.0f),
        },
    };

    // record text into glyph buffer and draw commands
    bool any_valid = state.fonts.cairo.valid || state.fonts.lucide.valid || state.fonts.twemoji.valid;
    if (any_valid) {
        begin_push_glyphs();
        if (state.fonts.cairo.valid) {
            for (int i = 0; i < 4; i++) {
                push_centered_line(&state.fonts.cairo, line[i], i);
            }
        }
        if (state.fonts.lucide.valid) {
            push_centered_line(&state.fonts.lucide, line[4], 4);
        }
        if (state.fonts.twemoji.valid) {
            push_centered_line_emoji(&state.fonts.twemoji, line[5], 5);
        }
        end_push_glyphs();
    }

    // render the recorded draw commands
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    if (any_valid) {
        sg_apply_pipeline(state.pip);
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
        for (int i = 0; i < state.draw.cur_draw_command; i++) {
            const draw_command_t* cmd = &draw_commands[i];
            sg_apply_bindings(&(sg_bindings){
                .vertex_buffers[0] = state.buf,
                .vertex_buffer_offsets[0] = cmd->base_instance * sizeof(glyph_vertex_t),
                .views = {
                    [VIEW_band_tex] = cmd->band_tex_view,
                    [VIEW_curve_tex] = cmd->curve_tex_view,
                },
                .samplers[SMP_point_sampler] = state.smp,
            });
            sg_draw(0, 6, cmd->num_instances);
        }
    }
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
                if (state.inp.zoom < MIN_ZOOM) {
                    state.inp.zoom = MIN_ZOOM;
                } else if (state.inp.zoom > MAX_ZOOM) {
                    state.inp.zoom = MAX_ZOOM;
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
    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        igText("Left mouse button and move mouse to pan.");
        igText("Mouse wheel to zoom.");
        igSeparator();
        igSliderFloat("Font Size", &state.font_size, 5.0f, 256.0f);
    }
    igEnd();
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
            total += glyph->advance * state.font_size;
        }
    }
    return total;
}

static void begin_push_glyphs(void) {
    state.draw.start_glyph_vertex = 0;
    state.draw.cur_glyph_vertex = 0;
    state.draw.cur_draw_command = 0;
    state.draw.cur_font = 0;
}

static void end_push_glyphs(void) {
    // push final draw command
    push_draw_command();
    // update the glyph instance buffer
    sg_update_buffer(state.buf, &(sg_range){
        .ptr = glyph_vertices,
        .size = state.draw.cur_glyph_vertex * sizeof(glyph_vertex_t),
    });
}

static void push_draw_command(void) {
    if ((state.draw.cur_draw_command < MAX_DRAW_COMMANDS) && (state.draw.cur_glyph_vertex > state.draw.start_glyph_vertex)) {
        assert(state.draw.cur_font);
        draw_commands[state.draw.cur_draw_command++] = (draw_command_t){
            .base_instance = state.draw.start_glyph_vertex,
            .num_instances = state.draw.cur_glyph_vertex - state.draw.start_glyph_vertex,
            .curve_tex_view = state.draw.cur_font->curve.tex_view,
            .band_tex_view = state.draw.cur_font->band.tex_view,
        };
        state.draw.start_glyph_vertex = state.draw.cur_glyph_vertex;
    }
}

static void push_glyph_vertex(const glyph_vertex_t* v) {
    if (state.draw.cur_glyph_vertex < MAX_DRAWN_GLYPHS) {
        glyph_vertices[state.draw.cur_glyph_vertex++] = *v;
    }
}

static void push_centered_line(const slug_font_t* font, const uint32_t* text, int line_nr) {
    const int total_lines = TOTAL_LINES;
    const float line_height = state.font_size * 1.5f;
    const float block_height = (float)total_lines * line_height;
    float line_width = measure_line(font, text);
    float base_x = (sapp_widthf() - line_width) * 0.5f;
    float base_y = (sapp_heightf() + block_height) * 0.5f - (float)line_nr * line_height;
    push_line(font, text, base_x, base_y);
}

static void push_centered_line_emoji(const slug_font_t* font, const uint32_t* text, int line_nr) {
    const int total_lines = TOTAL_LINES;
    const float line_height = state.font_size * 1.5f;
    const float block_height = (float)total_lines * line_height;
    float line_width = measure_line(font, text);
    float base_x = (sapp_widthf() - line_width) * 0.5f;
    float base_y = (sapp_heightf() + block_height) * 0.5f - (float)line_nr * line_height;
    push_line_emoji(font, text, base_x, base_y);
}

static void push_line(const slug_font_t* font, const uint32_t* text, float x, float y) {
    uint32_t cp = 0;
    while ((cp = *text++) != 0) {
        const slug_glyph_t* glyph = slug_get_glyph(font, cp);
        if (glyph) {
            push_glyph(font, glyph, x, y, vec4(1.0f, 1.0f, 1.0f, 1.0f));
            x += glyph->advance * state.font_size;
        }
    }
}

static void push_line_emoji(const slug_font_t* font, const uint32_t* text, float x, float y) {
    uint32_t cp = 0;
    while ((cp = *text++) != 0) {
        const slug_glyph_t* glyph = slug_get_glyph(font, cp);
        if (glyph) {
            push_emoji(font, cp, x, y);
            x += glyph->advance * state.font_size;
        }
    }
}

static void push_emoji(const slug_font_t* font, uint32_t codepoint, float x, float y) {
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
        push_glyph(font, glyph, x, y, color);
    }
}

static uint32_t pack_color_u32(vec4_t color) {
    uint32_t r = (uint32_t)(color.x * 255);
    uint32_t g = (uint32_t)(color.y * 255);
    uint32_t b = (uint32_t)(color.z * 255);
    uint32_t a = (uint32_t)(color.w * 255);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static void push_glyph(const slug_font_t* font, const slug_glyph_t* glyph, float x, float y, vec4_t color) {
    if ((glyph->max_band_x < 0.0f) || (glyph->max_band_y < 0.0f)) {
        return;
    }
    if (font != state.draw.cur_font) {
        if (state.draw.cur_font != 0) {
            push_draw_command();
        }
        state.draw.cur_font = font;
    }
    const glyph_vertex_t glyph_vertex = {
        .draw_rect = {
            x + (glyph->bbox.x0 * state.font_size),
            y + (glyph->bbox.y0 * state.font_size),
            (glyph->bbox.x1 - glyph->bbox.x0) * state.font_size,
            (glyph->bbox.y1 - glyph->bbox.y0) * state.font_size,
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
        .color = pack_color_u32(color),
    };
    push_glyph_vertex(&glyph_vertex);
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
