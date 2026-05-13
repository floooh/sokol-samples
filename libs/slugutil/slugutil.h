#include "sokol_gfx.h"
#include "stb_truetype.h"
#include <stdint.h>

#define SLUG_TEX_WIDTH (4096)
#define SLUG_MAX_BANDS (16)

typedef struct {
    const void* ptr;
    size_t size;
} slug_range_t;

typedef struct {
    float bbox[4];
    float advance;
    float lsb;
    float max_band_x;
    float max_band_y;
    float band_scale[2];
    float band_offset[2];
    int glyph_loc[2];
} slug_glyph_t;

typedef struct {
    uint16_t glyph_id;
    uint16_t palette_index;
} slug_colr_layer_t;

typedef struct {
    uint16_t first_layer;
    uint16_t num_layers;
} slug_colr_base_t;

typedef struct {
    uint16_t key;
    int index;
} slug_hashmap_item_t;

typedef struct {
    bool valid;
    struct {
        slug_glyph_t* ptr;
        int num;
    } glyphs;
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
    struct {
        sg_color* ptr;
        int num;
    } cpal_colors;
    struct {
        int num;
        slug_colr_base_t* ptr;
        slug_hashmap_item_t* hashmap; // keys are 'glyph_id'
    } colr_bases;
    struct {
        slug_colr_layer_t* ptr;
        int num;
    } colr_layers;
} slug_font_t;

bool slug_load_font(slug_font_t* font, float pixel_size, const slug_range_t* data);
void slug_unload_font(slug_font_t* font);
