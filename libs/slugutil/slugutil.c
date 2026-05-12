#include "slugutil.h"
#include <assert.h>
#include <stdlib.h>

static bool parse_colr_v0(slug_font_t* font, const slug_range_t* data);
static bool parse_cpal(slug_font_t* font, const slug_range_t* data);

bool slug_load_font(slug_font_t* font, float pixel_size, const slug_range_t* data) {
    assert(font);
    assert(pixel_size > 0.0f);
    assert(data && data->ptr && data->size > 0);
    assert(!font->valid);
    *font = (slug_font_t){0};

    if (!stbtt_InitFont(&font->info, data->ptr, 0)) {
        slug_unload_font(font);
        return false;
    }
    float truetype_scale = stbtt_ScaleForMappingEmToPixels(&font->info, pixel_size);

    // colored emoji-fonts...
    if (!parse_colr_v0(font, data)) {
        slug_unload_font(font);
        return false;
    }
    if (!parse_cpal(font, data)) {
        slug_unload_font(font);
        return false;
    }


    font->valid = true;
    return true;
}

void slug_unload_font(slug_font_t* font) {
    sg_destroy_image(font->curve.img);
    sg_destroy_view(font->curve.tex_view);
    sg_destroy_image(font->band.img);
    sg_destroy_view(font->band.tex_view);
    if (font->glyphs.ptr) {
        free(font->glyphs.ptr);
    }
    if (font->cpal_colors.ptr) {
        free(font->cpal_colors.ptr);
    }
    if (font->colr_bases.ptr) {
        free(font->colr_bases.ptr);
    }
    if (font->colr_layers.ptr) {
        free(font->colr_layers.ptr);
    }
    *font = (slug_font_t){0};
}


uint32_t make_tag(char a, char b, char c, char d) {
    return (a<<24) | (b<<16) | (c<<8) | d;
}

uint16_t read_u16be(const slug_range_t* data, size_t offset) {
    assert((offset + sizeof(uint16_t)) <= data->size);
    const uint8_t* p = (uint8_t*)data->ptr;
    uint32_t b0 = p[offset];
    uint32_t b1 = p[offset + 1];
    return (b0<<8) | b1;
}

uint32_t read_u32be(const slug_range_t* data, size_t offset) {
    assert((offset + sizeof(uint32_t)) <= data->size);
    const uint8_t* p = (uint8_t*)data->ptr;
    uint32_t b0 = p[offset];
    uint32_t b1 = p[offset + 1];
    uint32_t b2 = p[offset + 2];
    uint32_t b3 = p[offset + 3];
    return (b0<<24) | (b1<<16) | (b2<<8) | b3;
}

int find_otf_table(const slug_range_t* data, uint32_t tag) {
    if (data->size < 12) {
        return -1;
    }
    uint16_t num_tables = read_u16be(data, 4);
    for (uint16_t i = 0; i < num_tables; i++) {
        size_t record_offset = 12 + i * 16;
        if ((record_offset + 16) > data->size) {
            break;
        }
        uint32_t record_tag = read_u32be(data, record_offset);
        if (record_tag == tag) {
            return (int)read_u32be(data, record_offset + 8);
        }
    }
    return -1;
}

static bool parse_colr_v0(slug_font_t* font, const slug_range_t* data) {
    int table_offset = find_otf_table(data, make_tag('C', 'O', 'L', 'R'));
    if (table_offset < 0) {
        // not a but, COLR table is optional
        return true;
    }
    uint16_t version = read_u16be(data, table_offset);
	// dont support COLRv1
    if (version != 0) {
        return false;
    }
	//Table header (14 bytes from table start):
	// 	Offset +0:  u16 version               (must be 0 for COLRv0)
	// 	Offset +2:  u16 numBaseGlyphRecords
	// 	Offset +4:  u32 offsetBaseGlyphRecord  (from table start)
	// 	Offset +8:  u32 offsetLayerRecord      (from table start)
	// 	Offset +12: u16 numLayerRecords
    int num_base_glyphs = (int)read_u16be(data, table_offset + 2);
    int offset_base = table_offset + (int)read_u32be(data, table_offset + 4);
    int offset_layer = table_offset + (int)read_u32be(data, table_offset + 8);
    int num_layers = (int)read_u16be(data, table_offset + 12);
    font->colr_bases.num = num_base_glyphs;
    font->colr_bases.ptr = calloc((size_t)num_base_glyphs, sizeof(slug_colr_base_t));
    for (int i = 0; i < num_base_glyphs; i++) {
        slug_colr_base_t* ptr = &font->colr_bases.ptr[i];
        int offset = offset_base + i * 6; // each record is 6 bytes
        //[glyphID: u16, firstLayerIndex: u16, numLayers: u16]
        ptr->glyph_id = read_u16be(data, offset);
        ptr->first_layer = read_u16be(data, offset + 2);
        ptr->num_layers = read_u16be(data, offset + 4);
    }
    font->colr_layers.num = num_layers;
    font->colr_layers.ptr = calloc((size_t)num_layers, sizeof(slug_colr_layer_t));
    for (int i = 0; i < num_layers; i++) {
        slug_colr_layer_t* ptr = &font->colr_layers.ptr[i];
        int offset = offset_layer + i * 4; // each record is 4 bytes
        ptr->glyph_id = read_u16be(data, offset);
        ptr->palette_index = read_u16be(data, offset + 2);
    }
    return true;
}

static bool parse_cpal(slug_font_t* font, const slug_range_t* data) {
    return false;
}

static bool parse_colr_v0(slug_font_t* font, const slug_range_t* data);
static bool parse_cpal(slug_font_t* font, const slug_range_t* data);
