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
#include "slug-sapp.glsl.h"

#define MAX_FONTS (3)
#define MAX_FONT_SIZE (2 * 1024 * 1024)

const char* l0_ascii = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

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
} state;

uint8_t file_buffers[MAX_FONTS][MAX_FONT_SIZE];

static void draw_ui(void);
static float measure_line_ascii(const slug_font_t* font, const char* text);
static void draw_line_ascii(const slug_font_t* font, const char* text, float x, float y, const mat44_t* mvp);
static void draw_glyph(const slug_glyph_t* glyph, float x, float y, const mat44_t* mvp, vec4_t color);
static void cairo_callback(const sfetch_response_t* response);
static void lucide_callback(const sfetch_response_t* response);
static void twemoji_callback(const sfetch_response_t* response);

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
        .callback = cairo_callback,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("lucide.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[1], .size = sizeof(file_buffers[1]) },
        .callback = lucide_callback,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("twemoji.ttf", buf, sizeof(buf)),
        .buffer = { .ptr = file_buffers[2], .size = sizeof(file_buffers[2]) },
        .callback = twemoji_callback,
    });
}

static void frame(void) {
    sfetch_dowork();
    draw_ui();

    float w = sapp_widthf();
    float h = sapp_heightf();

    // FIXME: replace with mat44_ortho func
    float sx = 2.0f / (w / state.inp.zoom);
    float sy = 2.0f / (h / state.inp.zoom);
    float tx = -1.0f - state.inp.pan_x * sx;
    float ty = -1.0f - state.inp.pan_y * sy;
    mat44_t mvp = {
        .x = vec4(sx, 0.0f, 0.0f, 0.0f),
        .y = vec4(0.0f, sy, 0.0f, 0.0f),
        .z = vec4(0.0f, 0.0f, -1.0f, 0.0f),
        .w = vec4(tx, ty, 0.0f, 1.0f),
    };
    float line_height = 60.0f;

    int total_lines = 6;
    float block_height = (float)total_lines * line_height;

    bool any_valid = state.fonts.cairo.valid || state.fonts.lucide.valid || state.fonts.twemoji.valid;
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    if (any_valid) {
        sg_apply_pipeline(state.pip);
    }
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
        float l0_ascii_width = measure_line_ascii(&state.fonts.cairo, l0_ascii);
        float max_width = l0_ascii_width;
        float base_x = (w - max_width) * 0.5f;
        float base_y = (h + block_height) * 0.5f - line_height;
        float cursor_y = base_y;
        float lw = measure_line_ascii(&state.fonts.cairo, l0_ascii);
        float line_x = base_x + (max_width = lw) * 0.5f;
        draw_line_ascii(&state.fonts.cairo, l0_ascii, line_x, cursor_y, &mvp);
    }
    if (state.fonts.lucide.valid) {
        // FIXME: draw lucide characters
    }
    if (state.fonts.twemoji.valid) {
        // FIXME: draw twemoji characters
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
                float mouse_world_x = state.inp.pan_x + ev->mouse_x / state.inp.zoom;
                float mouse_world_y = state.inp.pan_y + ev->mouse_y / state.inp.zoom;
                state.inp.zoom *= 1.0 + ev->scroll_y * 0.1f;
                if (state.inp.zoom < 0.1f) {
                    state.inp.zoom = 0.1f;
                } else if (state.inp.zoom > 50.0f) {
                    state.inp.zoom = 50.0f;
                }
                // Adjust pan so the world point under the mouse stays fixed
                state.inp.pan_x = mouse_world_x - ev->mouse_x / state.inp.zoom;
                state.inp.pan_y = mouse_world_y - ev->mouse_y / state.inp.zoom;
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

static void cairo_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.cairo, 48.0f, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static void lucide_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.lucide, 48.0f, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static void twemoji_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        slug_load_font(&state.fonts.twemoji, 48.0f, &(slug_range_t){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
    }
}

static float measure_line_ascii(const slug_font_t* font, const char* text) {
    float total = 0.0f;
    char c;
    while ((c = *text++) != 0) {
        const slug_glyph_t* glyph = slug_get_glyph(font, (uint32_t)c);
        if (glyph) {
            total += glyph->advance;
        }
    }
    return total;
}

static void draw_line_ascii(const slug_font_t* font, const char* text, float x, float y, const mat44_t* mvp) {
    char c = 0;
    while ((c = *text++) != 0) {
        const slug_glyph_t* glyph = slug_get_glyph(font, (uint32_t)c);
        if (glyph) {
            draw_glyph(glyph, x, y, mvp, vec4(1.0f, 1.0f, 1.0f, 1.0f));
            x += glyph->advance;
        }
    }
}

static void draw_glyph(const slug_glyph_t* glyph, float x, float y, const mat44_t* mvp, vec4_t color) {
    if ((glyph->max_band_x < 0.0f) || (glyph->max_band_y < 0.0f)) {
        return;
    }
    const vs_params_t vs_params = {
        .mvp = *mvp,
        .draw_rect = {
            .x = x + glyph->bbox.x0,
            .y = y + glyph->bbox.y0,
            .z = glyph->bbox.x1 - glyph->bbox.x0,
            .w = glyph->bbox.y1 - glyph->bbox.y0,
        },
        .glyph_bbox = {
            .x = glyph->bbox.x0,
            .y = glyph->bbox.y0,
            .z = glyph->bbox.x1,
            .w = glyph->bbox.y1,
        }
    };
    const fs_params_t fs_params = {
        .text_color = color,
        .band_transform = {
            .x = glyph->band_scale.x,
            .y = glyph->band_scale.y,
            .z = glyph->band_offset.x,
            .w = glyph->band_offset.y,
        },
        .glyph_params = {
            [0] = glyph->glyph_loc[0],
            [1] = glyph->glyph_loc[1],
            [2] = (int)glyph->max_band_x,
            [3] = (int)glyph->max_band_y,
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
        .width = 800,
        .height = 600,
        .sample_count = 1,
        .high_dpi = true,
        .window_title = "slug-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
