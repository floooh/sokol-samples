#include "sokol_gfx.h"
#include "stb_truetype.h"
#include <stdint.h>
#define VECMATH_GENERICS
#include "../vecmath/vecmath.h"

#define SLUG_TEX_WIDTH (4096)
#define SLUG_MAX_BANDS (16)

typedef struct {
    vec2_t p[3];
    uint16_t texture[2];
} slug_curve_t;

typedef struct {
    int start;
    int count;
} slug_contour_range_t;

typedef struct {
    int curve_index;
    float sort_key;
} slug_band_entry_t;

typedef struct {
    float x0, y0, x1, y1;
} slug_bbox_t;

typedef struct {
    slug_curve_t* curves;           // managed via stb_ds
    slug_contour_range_t* contours; // managed via stb_ds
    slug_bbox_t bbox;
    float advance;
    float lsb;
    slug_band_entry_t** horizontal_bands;    // managed via stb_ds
    slug_band_entry_t** vertical_bands;      // managed via stb_ds
    vec2_t band_scale;
    vec2_t band_offset;
    int32_t glyph_loc[2];
} slug_glyph_build_t;

typedef struct {
    const void* ptr;
    size_t size;
} slug_range_t;

typedef struct {
    slug_bbox_t bbox;
    float advance;
    float lsb;
    float max_band_x;
    float max_band_y;
    vec2_t band_scale;
    vec2_t band_offset;
    int glyph_loc[2];
} slug_glyph_t;

typedef struct {
    uint16_t glyph_id;
    uint16_t palette_index;
} slug_colr_layer_t;

typedef struct {
    uint16_t glyph_id;      // this also serves as hashmap key
    uint16_t first_layer;
    uint16_t num_layers;
    uint16_t _pad;
} slug_colr_base_t;

typedef struct {
    bool valid;
    slug_glyph_t* glyphs;   // managed via stb_ds
    stbtt_fontinfo info;
    struct {
        sg_image img;
        sg_view tex_view;
        int height;
    } curve;
    struct {
        sg_image img;
        sg_view tex_view;
        int height;
    } band;
    vec4_t* cpal_colors;              // managed via stb_ds
    slug_colr_base_t* colr_bases;     // managed via stb_ds
    slug_colr_layer_t* colr_layers;   // managed via stb_ds;
} slug_font_t;

bool slug_load_font(slug_font_t* font, const slug_range_t* data);
void slug_unload_font(slug_font_t* font);
const slug_glyph_t* slug_get_glyph(const slug_font_t* font, uint32_t cp);
const slug_colr_base_t* slug_find_colr_base(const slug_font_t* font, uint32_t cp);
