//------------------------------------------------------------------------------
//  Slug utility functions.
//  NOTE: memory management during font loading is *really* unoptimized.
//
//  PS: should the font processing actually be moved into an offline tool?
//------------------------------------------------------------------------------

#include "slugutil.h"
#include <assert.h>
#include <stdlib.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef struct { uint32_t x, y, z, w; } uvec4_t;

typedef struct {
    vec4_t* curve_pixels;     // managed by stb_ds
    int curve_height;
    uvec4_t* band_pixels;     // managed by stb_ds
    int band_height;
} pack_textures_t;

static bool parse_colr_v0(slug_font_t* font, const slug_range_t* data);
static bool parse_cpal(slug_font_t* font, const slug_range_t* data);
static void init_build_glyph(const stbtt_fontinfo* info, int glyph_index, float scale, slug_glyph_build_t* out);
static void build_bands(slug_glyph_build_t* glyph);
static void free_build_glyph(slug_glyph_build_t* glyph);
static pack_textures_t pack_textures(slug_glyph_build_t* glyphs, int num_glyphs);

const slug_glyph_t* slug_get_glyph(const slug_font_t* font, uint32_t codepoint) {
    int idx = stbtt_FindGlyphIndex(&font->info, codepoint);
    if ((idx >= 0) && (idx < arrlen(font->glyphs))) {
        return &font->glyphs[idx];
    } else {
        return 0;
    }
}

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

    slug_glyph_build_t* build_glyphs = 0;
    arrsetlen(build_glyphs, font->info.numGlyphs);
    for (int i = 0; i < arrlen(build_glyphs); i++) {
        init_build_glyph(&font->info, i, truetype_scale, &build_glyphs[i]);
        build_bands(&build_glyphs[i]);
    }

    pack_textures_t res = pack_textures(build_glyphs, arrlen(build_glyphs));
    font->curve.height = res.curve_height;
    font->band.height = res.band_height;

    font->curve.img = sg_make_image(&(sg_image_desc){
        .width = SLUG_TEX_WIDTH,
        .height = font->curve.height,
        .pixel_format = SG_PIXELFORMAT_RGBA32F,
        .data.mip_levels[0] = {
            .ptr = res.curve_pixels,
            .size = arrlen(res.curve_pixels) * sizeof(vec4_t),
        },
    });
    font->curve.tex_view = sg_make_view(&(sg_view_desc){ .texture.image = font->curve.img });

    font->band.img = sg_make_image(&(sg_image_desc){
        .width = SLUG_TEX_WIDTH,
        .height = font->band.height,
        .pixel_format = SG_PIXELFORMAT_RGBA32UI,
        .data.mip_levels[0] = {
            .ptr = res.band_pixels,
            .size = arrlen(res.band_pixels) * sizeof(uvec4_t),
        },
    });
    font->band.tex_view = sg_make_view(&(sg_view_desc){ .texture.image = font->band.img });

    int num_glyphs = arrlen(build_glyphs);
    arrsetlen(font->glyphs, num_glyphs);
    for (int i = 0; i < num_glyphs; i++) {
        const slug_glyph_build_t* bg = &build_glyphs[i];
        font->glyphs[i] = (slug_glyph_t){
            .bbox = bg->bbox,
            .advance = bg->advance,
            .lsb = bg->lsb,
            .max_band_x = arrlen(bg->vertical_bands) - 1,
            .max_band_y = arrlen(bg->horizontal_bands) - 1,
            .band_scale = bg->band_scale,
            .band_offset = bg->band_offset,
            .glyph_loc = {
                [0] = bg->glyph_loc[0],
                [1] = bg->glyph_loc[1],
            }
        };
    }


    arrfree(res.curve_pixels);
    arrfree(res.band_pixels);
    for (int i = 0; i < arrlen(build_glyphs); i++) {
        free_build_glyph(&build_glyphs[i]);
    }
    arrfree(build_glyphs);
    font->valid = true;
    return true;
}

void slug_unload_font(slug_font_t* font) {
    sg_destroy_image(font->curve.img);
    sg_destroy_view(font->curve.tex_view);
    sg_destroy_image(font->band.img);
    sg_destroy_view(font->band.tex_view);
    arrfree(font->glyphs);
    arrfree(font->cpal_colors);
    arrfree(font->colr_bases);
    arrfree(font->colr_layers);
    *font = (slug_font_t){0};
}


static uint32_t make_tag(char a, char b, char c, char d) {
    return (a<<24) | (b<<16) | (c<<8) | d;
}

static uint16_t read_u16be(const slug_range_t* data, size_t offset) {
    assert((offset + sizeof(uint16_t)) <= data->size);
    const uint8_t* p = (uint8_t*)data->ptr;
    uint32_t b0 = p[offset];
    uint32_t b1 = p[offset + 1];
    return (b0<<8) | b1;
}

static uint32_t read_u32be(const slug_range_t* data, size_t offset) {
    assert((offset + sizeof(uint32_t)) <= data->size);
    const uint8_t* p = (uint8_t*)data->ptr;
    uint32_t b0 = p[offset];
    uint32_t b1 = p[offset + 1];
    uint32_t b2 = p[offset + 2];
    uint32_t b3 = p[offset + 3];
    return (b0<<24) | (b1<<16) | (b2<<8) | b3;
}

static int find_otf_table(const slug_range_t* data, uint32_t tag) {
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

static int colr_base_cmp(const void* a, const void* b) {
    const slug_colr_base_t* pa = (slug_colr_base_t*)a;
    const slug_colr_base_t* pb = (slug_colr_base_t*)b;
    if (pa->glyph_id < pb->glyph_id) {
        return -1;
    } else if (pa->glyph_id > pb->glyph_id) {
        return 1;
    } else {
        return 0;
    }
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
    arrsetlen(font->colr_bases, num_base_glyphs);
    for (int i = 0; i < num_base_glyphs; i++) {
        slug_colr_base_t* ptr = &font->colr_bases[i];
        int offset = offset_base + i * 6; // each record is 6 bytes
        //[glyphID: u16, firstLayerIndex: u16, numLayers: u16]
        ptr->glyph_id = read_u16be(data, offset);
        ptr->first_layer = read_u16be(data, offset + 2);
        ptr->num_layers = read_u16be(data, offset + 4);
        ptr->_pad = 0;
    }
    qsort(font->colr_bases, arrlen(font->colr_bases), sizeof(slug_colr_base_t), colr_base_cmp);
    arrsetlen(font->colr_layers, num_layers);
    for (int i = 0; i < num_layers; i++) {
        slug_colr_layer_t* ptr = &font->colr_layers[i];
        int offset = offset_layer + i * 4; // each record is 4 bytes
        ptr->glyph_id = read_u16be(data, offset);
        ptr->palette_index = read_u16be(data, offset + 2);
    }
    return true;
}

static int mini(int a, int b) {
    return a < b ? a : b;
}

static int clampi(int val, int minval, int maxval) {
    if (val < minval) return minval;
    else if (val > maxval) return maxval;
    else return val;
}

static float minf(float a, float b) {
    return a < b ? a : b;
}

static float maxf(float a, float b) {
    return a > b ? a : b;
}

static bool parse_cpal(slug_font_t* font, const slug_range_t* data) {
    int table_offset = find_otf_table(data, make_tag('C', 'P', 'A', 'L'));
    if (table_offset < 0) {
        // not a bug, CPAL is optional
        return true;
    }
    // Table header (12 bytes from table start):
    //   Offset +0: u16 version
    //   Offset +2: u16 numPaletteEntries   (number of colors per palette)
    //   Offset +4: u16 numPalettes         (usually 1)
    //   Offset +6: u16 numColorRecords     (total colors across all palettes)
    //   Offset +8: u32 offsetFirstColorRecord  (from table start)
    int num_entries = (int)read_u16be(data, table_offset + 2);
    int num_color_records = (int)read_u16be(data, table_offset + 6);
    int color_offset = table_offset + (int)read_u32be(data, table_offset + 8);
    int count = mini(num_entries, num_color_records);
    arrsetlen(font->cpal_colors, count);
    for (int i = 0; i < count; i++) {
        const uint8_t* src = (uint8_t*)data->ptr + color_offset + i * 4;
        vec4_t* dst = &font->cpal_colors[i];
        dst->z = ((float)*src++) / 255.0f;
        dst->y = ((float)*src++) / 255.0f;
        dst->x = ((float)*src++) / 255.0f;
        dst->w = ((float)*src) / 255.0f;
    }
    return true;
}

static void free_build_glyph(slug_glyph_build_t* glyph) {
    arrfree(glyph->curves);
    arrfree(glyph->contours);
    for (int i = 0; i < arrlen(glyph->horizontal_bands); i++) {
        arrfree(glyph->horizontal_bands[i]);
    }
    arrfree(glyph->horizontal_bands);
    for (int i = 0; i < arrlen(glyph->vertical_bands); i++) {
        arrfree(glyph->vertical_bands[i]);
    }
    arrfree(glyph->vertical_bands);
}

static void init_build_glyph(const stbtt_fontinfo* info, int glyph_index, float scale, slug_glyph_build_t* out) {
    slug_glyph_build_t* glyph = out;
    memset(glyph, 0, sizeof(slug_glyph_build_t));
    int adv, lsb_raw;
    stbtt_GetGlyphHMetrics(info, glyph_index, &adv, &lsb_raw);
    glyph->advance = (float)adv * scale;
    glyph->lsb = (float)lsb_raw * scale;

    int ix0, iy0, ix1, iy1;
    if (stbtt_GetGlyphBox(info, glyph_index, &ix0, &iy0, &ix1, &iy1) == 0) {
        return;
    }
    glyph->bbox = (slug_bbox_t){
        .x0 = (float)ix0 * scale,
        .y0 = (float)iy0 * scale,
        .x1 = (float)ix1 * scale,
        .y1 = (float)iy1 * scale,
    };

    stbtt_vertex* verts;
    int nv = stbtt_GetGlyphShape(info, glyph_index, &verts);
    if (nv <= 0) {
        return;
    }
    bool in_contour = false;
    int contour_start = 0;
    vec2_t previous = vec2(0.0f, 0.0f);
    for (int i = 0; i < nv; i++) {
        stbtt_vertex* vert = &verts[i];
        switch (vert->type) {
            case 1:
                // vmove
                {
                    if (in_contour) {
                        int count = arrlen(glyph->curves) - contour_start;
                        if (count > 0) {
                            arrput(glyph->contours, ((slug_contour_range_t){ .start = contour_start, .count = count }));
                        }
                    }
                    previous = vec2((float)vert->x * scale, (float)vert->y * scale);
                    contour_start = arrlen(glyph->curves);
                    in_contour = true;
                }
                break;
            case 2:
                // vline
                {
                    vec2_t current = vec2((float)vert->x * scale, (float)vert->y * scale);
                    vec2_t mid = vm_mul(vm_add(previous, current), 0.5f);
                    arrput(glyph->curves, ((slug_curve_t){ .p = { previous, mid, current } }));
                    previous = current;
                }
                break;
            case 3:
                // vcurve
                {
                    vec2_t current = vec2((float)vert->x * scale, (float)vert->y * scale);
                    vec2_t control = vec2((float)vert->cx * scale, (float)vert->cy * scale);
                    arrput(glyph->curves, ((slug_curve_t){ .p = { previous, control, current }}));
                    previous= current;
                }
                break;
            case 4:
                // vcubic — approximate with three quadratic Beziers
                // Split cubic P0,C1,C2,P3 at t=1/3 and t=2/3 via de Casteljau,
                // then approximate each sub-cubic as a quadratic with ctrl=(c1+c2)/2.
                {
                    vec2_t p3 = vec2((float)vert->x * scale, (float)vert->y * scale);
                    vec2_t c1 = vec2((float)vert->cx * scale, (float)vert->cy * scale);
                    vec2_t c2 = vec2((float)vert->cx1 * scale, (float)vert->cy1 * scale);
                    vec2_t p0 = previous;
                    // de Casteljau split at t=1/3
                    const float t = 1.0f / 3.0f;
                    vec2_t ab = vm_add(p0, vm_mul(vm_sub(c1, p0), t));
                    vec2_t bc = vm_add(c1, vm_mul(vm_sub(c2, c1), t));
                    vec2_t cd = vm_add(c2, vm_mul(vm_sub(p3, c2), t));
                    vec2_t abc = vm_add(ab, vm_mul(vm_sub(bc, ab), t));
                    vec2_t bcd = vm_add(bc, vm_mul(vm_sub(cd, bc), t));
                    vec2_t e1 = vm_add(abc, vm_mul(vm_sub(bcd, abc), t)); // point on curve at t=1/3
                    // Sub-cubic 1: p0, ab, abc, e1 → quadratic ctrl = (ab + abc) * 0.5
                    vec2_t q1 = vm_mul(vm_add(ab, abc), 0.5f);
                    // de Casteljau split remaining cubic (e1, bcd, cd, p3) at t=0.5 (= t=2/3 of original)
                    vec2_t ab2 = vm_add(e1, vm_mul(vm_sub(bcd, e1), 0.5f));
                    vec2_t bc2 = vm_add(bcd, vm_mul(vm_sub(cd, bcd), 0.5f));
                    vec2_t cd2 = vm_add(cd, vm_mul(vm_sub(p3, cd), 0.5f));
                    vec2_t abc2 = vm_add(ab2, vm_mul(vm_sub(p3, cd), 0.5f));
                    vec2_t bcd2 = vm_add(bc2, vm_mul(vm_sub(cd2, bc2), 0.5f));
                    vec2_t e2 = vm_add(abc2, vm_mul(vm_sub(bcd2, abc2), 0.5f)); // point on curve at t=2/3
                    // Sub-cubic 2: e1, ab2, abc2, e2 → quadratic ctrl = (ab2 + abc2) * 0.5
                    vec2_t q2 = vm_mul(vm_add(ab2, abc2), 0.5f);
                    vec2_t q3 = vm_mul(vm_add(bcd2, cd2), 0.5f);
                    arrput(glyph->curves, ((slug_curve_t){ .p = { p0, q1, e1 } }));
                    arrput(glyph->curves, ((slug_curve_t){ .p = { e1, q2, e2 } }));
                    arrput(glyph->curves, ((slug_curve_t){ .p = { e2, q3, p3 } }));
                    previous = p3;
                }
                break;
        }
    }
    if (in_contour) {
        int count = arrlen(glyph->curves) - contour_start;
        if (count > 0) {
            arrput(glyph->contours, ((slug_contour_range_t){ .start = contour_start, .count = count }));
        }
    }
    stbtt_FreeShape(info, verts);
}

static int band_cmp(const void* a, const void* b) {
    const slug_band_entry_t* pa = (slug_band_entry_t*)a;
    const slug_band_entry_t* pb = (slug_band_entry_t*)b;
    // NOTE: inverted sort-order is not a bug
    if (pa->sort_key > pb->sort_key) {
        return -1;
    } else if (pa->sort_key < pb->sort_key) {
        return 1;
    } else {
        return 0;
    }
}

static void build_bands(slug_glyph_build_t* glyph) {
    int num_curves = arrlen(glyph->curves);
    if (0 == num_curves) {
        return;
    }

    float band_width = maxf(glyph->bbox.x1 - glyph->bbox.x0, 1.0f);
    float band_height = maxf(glyph->bbox.y1 - glyph->bbox.y0, 1.0f);
    int number_of_bands_height = clampi(num_curves, 1, SLUG_MAX_BANDS);
    int number_of_bands_width = clampi(num_curves, 1, SLUG_MAX_BANDS);

    arrsetlen(glyph->horizontal_bands, number_of_bands_height);
    arrsetlen(glyph->vertical_bands, number_of_bands_width);
    for (int i = 0; i < number_of_bands_height; i++) {
        glyph->horizontal_bands[i] = 0;
    }
    for (int i = 0; i < number_of_bands_width; i++) {
        glyph->vertical_bands[i] = 0;
    }
    glyph->band_scale = vec2((float)number_of_bands_width / band_width, (float)number_of_bands_height / band_height);
    glyph->band_offset = vec2(-glyph->bbox.x0 * glyph->band_scale.x, -glyph->bbox.y0 * glyph->band_scale.y);

    float horizontal_band_height = band_height / (float)number_of_bands_height;
    float vertical_band_width = band_width / (float)number_of_bands_width;
    float horizontal_pad = horizontal_band_height * 0.5f;
    float vertical_pad = vertical_band_width * 0.5f;

    int band_first, band_last;
    for (int curve_index = 0; curve_index < arrlen(glyph->curves); curve_index++) {
        slug_curve_t* curve = &glyph->curves[curve_index];
        float curve_y_min = minf(minf(curve->p[0].y, curve->p[1].y), curve->p[2].y);
        float curve_y_max = maxf(maxf(curve->p[0].y, curve->p[1].y), curve->p[2].y);
		float curve_x_min = minf(minf(curve->p[0].x, curve->p[1].x), curve->p[2].x);
		float curve_x_max = maxf(maxf(curve->p[0].x, curve->p[1].x), curve->p[2].x);

        band_first = clampi(
            (int)floorf((curve_y_min - horizontal_pad - glyph->bbox.y0) / horizontal_band_height),
            0,
            number_of_bands_height - 1);
        band_last = clampi(
            (int)floorf((curve_y_max + horizontal_pad - glyph->bbox.y0) / horizontal_band_height),
            0,
            number_of_bands_height - 1);
        for (int i = band_first; i <= band_last; i++) {
            arrput(glyph->horizontal_bands[i], ((slug_band_entry_t){ .curve_index = curve_index, .sort_key = curve_x_max }));
        }

        band_first = clampi(
            (int)floorf((curve_x_min - vertical_pad - glyph->bbox.x0) / vertical_band_width),
            0,
            number_of_bands_width -1);
        band_last = clampi(
            (int)floorf((curve_x_max + vertical_pad - glyph->bbox.x0) / vertical_band_width),
            0,
            number_of_bands_width - 1);
        for (int i = band_first; i <= band_last; i++) {
            arrput(glyph->vertical_bands[i], ((slug_band_entry_t){ .curve_index = curve_index, .sort_key = curve_y_max }));
        }
    }
    qsort(glyph->horizontal_bands, arrlen(glyph->horizontal_bands), sizeof(slug_band_entry_t), band_cmp);
    qsort(glyph->vertical_bands, arrlen(glyph->vertical_bands), sizeof(slug_band_entry_t), band_cmp);
}

static void pad_to_row_curve_pixels(pack_textures_t* res, int needed) {
    int curlen = arrlen(res->curve_pixels);
    int column = curlen % SLUG_TEX_WIDTH;
    if ((column + needed) > SLUG_TEX_WIDTH) {
        arrsetlen(res->curve_pixels, curlen + SLUG_TEX_WIDTH - column);
        int newlen = arrlen(res->curve_pixels);
        for (int i = curlen; i < newlen; i++) {
            res->curve_pixels[i] = (vec4_t){0};
        }
    }
}

static void finalize_curve_pixels(pack_textures_t* res) {
    int cur_size = arrlen(res->curve_pixels);
    int new_size = 0;
    if (cur_size == 0) {
        arrsetlen(res->curve_pixels, SLUG_TEX_WIDTH);
        new_size = arrlen(res->curve_pixels);
        res->curve_height = 1;
    } else {
        res->curve_height = (arrlen(res->curve_pixels) + SLUG_TEX_WIDTH - 1) / SLUG_TEX_WIDTH;
        arrsetlen(res->curve_pixels, res->curve_height * SLUG_TEX_WIDTH);
        new_size = arrlen(res->curve_pixels);
    }
    for (int i = cur_size; i < new_size; i++) {
        res->curve_pixels[i] = (vec4_t){0};
    }
}

static void pad_to_row_band_pixels(pack_textures_t* res, int needed) {
    int curlen = arrlen(res->band_pixels);
    int column = curlen % SLUG_TEX_WIDTH;
    if ((column + needed) > SLUG_TEX_WIDTH) {
        arrsetlen(res->band_pixels, curlen + SLUG_TEX_WIDTH - column);
        int newlen = arrlen(res->band_pixels);
        for (int i = curlen; i < newlen; i++) {
            res->band_pixels[i] = (uvec4_t){0};
        }
    }
}

static void finalize_band_pixels(pack_textures_t* res) {
    int cur_size = arrlen(res->band_pixels);
    int new_size = 0;
    if (cur_size == 0) {
        arrsetlen(res->band_pixels, SLUG_TEX_WIDTH);
        new_size = arrlen(res->band_pixels);
        res->band_height = 1;
    } else {
        res->band_height = (arrlen(res->band_pixels) + SLUG_TEX_WIDTH - 1) / SLUG_TEX_WIDTH;
        arrsetlen(res->band_pixels, res->band_height * SLUG_TEX_WIDTH);
        new_size = arrlen(res->band_pixels);
    }
    for (int i = cur_size; i < new_size; i++) {
        res->band_pixels[i] = (uvec4_t){0};
    }
}

static void write_band_set(slug_band_entry_t** bands, slug_curve_t* curves, uvec4_t* pixels, int glyph_start, int header_offset, int* write_offset) {
    // Write headers: each band stores (count, data_offset) where data_offset
    // is relative to glyph_start, matching how the shader indexes into the texture.
    int data_offset = *write_offset;
    for (int band_index = 0; band_index < arrlen(bands); band_index++) {
        slug_band_entry_t* band = bands[band_index];
        uvec4_t pixel = { arrlen(band), (uint32_t)data_offset, 0, 0 };
        pixels[glyph_start + header_offset + band_index] = pixel;
        data_offset += arrlen(band);
    }
	// Write curve references at the offsets declared above
    data_offset = *write_offset;
    for (int band_index = 0; band_index < arrlen(bands); band_index++) {
        slug_band_entry_t* band = bands[band_index];
        for (int entry_index = 0; entry_index < arrlen(band); entry_index++) {
            slug_band_entry_t* entry = &band[entry_index];
            slug_curve_t* curve = &curves[entry->curve_index];
            uvec4_t pixel = { curve->texture[0], curve->texture[1], 0, 0 };
            pixels[glyph_start + data_offset] = pixel;
            data_offset += 1;
        }
    }
    *write_offset = data_offset;
}

static pack_textures_t pack_textures(slug_glyph_build_t* glyphs, int num_glyphs) {
    pack_textures_t res = {0};

	// Count how many texels we'll need so we can reserve upfront.
	// This avoids repeated realloc+copy as the dynamic arrays grow.
	int estimated_curve_size = 0;
	int estimated_band_size = 0;
    for (int glyph_index = 0; glyph_index < num_glyphs; glyph_index++) {
        slug_glyph_build_t* glyph = &glyphs[glyph_index];
		// Each contour needs (count + 1) curve texels:
		//   count = number of bezier curves in this contour
		//   +1 for the shared endpoint texel
        for (int contour_index = 0; contour_index < arrlen(glyph->contours); contour_index++) {
            slug_contour_range_t* contour = &glyph->contours[contour_index];
            estimated_curve_size += contour->count + 1;
        }
        int num_h = arrlen(glyph->horizontal_bands);
        int num_v = arrlen(glyph->vertical_bands);
        if ((num_h == 0) && (num_v == 0)) {
            continue;
        }
        // Band data per glyph:
		//   num_h + num_v = one header texel per band
		//   + sum of all curve references in each band
		int band_size = num_h + num_v;
        for (int i = 0; i < arrlen(glyph->horizontal_bands); i++) {
            band_size += arrlen(glyph->horizontal_bands[i]);
        }
        for (int i = 0; i < arrlen(glyph->vertical_bands); i++) {
            band_size += arrlen(glyph->vertical_bands[i]);
        }
		estimated_band_size += band_size;
    }
    arrsetcap(res.curve_pixels, (estimated_curve_size * 6) / 5);
    arrsetcap(res.band_pixels, (estimated_band_size * 6) / 5);

    for (int glyph_index = 0; glyph_index < num_glyphs; glyph_index++) {
        slug_glyph_build_t* glyph = &glyphs[glyph_index];
        // Pack curves into texture, recording each curve's texture coordinates
        for (int contour_index = 0; contour_index < arrlen(glyph->contours); contour_index++) {
            slug_contour_range_t* contour = &glyph->contours[contour_index];
            int entries_needed = contour->count + 1;
            pad_to_row_curve_pixels(&res, entries_needed);
            for (int i = 0; i < contour->count; i++) {
                slug_curve_t* curve = &glyph->curves[contour->start + i];
                int pixel_index = arrlen(res.curve_pixels);
                arrput(res.curve_pixels, vec4(curve->p[0].x, curve->p[0].y, curve->p[1].x, curve->p[1].y));
                curve->texture[0] = (uint32_t)(pixel_index % SLUG_TEX_WIDTH);
                curve->texture[1] = (uint32_t)(pixel_index / SLUG_TEX_WIDTH);
            }
            slug_curve_t* last_curve = &glyph->curves[contour->start + contour->count - 1];
            arrput(res.curve_pixels, vec4(last_curve->p[2].x, last_curve->p[2].y, 0.0f, 0.0f));
        }

		// Pack band lookup tables into texture, referencing the curve coords set above
        int num_h_bands = arrlen(glyph->horizontal_bands);
        int num_v_bands = arrlen(glyph->vertical_bands);
        if ((num_h_bands == 0) && (num_v_bands == 0)) {
            continue;
        }
        int header_size = num_h_bands + num_v_bands;
        pad_to_row_band_pixels(&res, header_size);

        int glyph_start = arrlen(res.band_pixels);
        glyph->glyph_loc[0] = (int32_t)glyph_start % SLUG_TEX_WIDTH;
        glyph->glyph_loc[1] = (int32_t)glyph_start / SLUG_TEX_WIDTH;

        int total_entries = header_size;
        for (int i = 0; i < arrlen(glyph->horizontal_bands); i++) {
            total_entries += arrlen(glyph->horizontal_bands[i]);
        }
        for (int i = 0; i < arrlen(glyph->vertical_bands); i++) {
            total_entries += arrlen(glyph->vertical_bands[i]);
        }
        arrsetlen(res.band_pixels, glyph_start + total_entries);

        int write_offset = header_size;
        write_band_set(
            glyph->horizontal_bands,
            glyph->curves,
            res.band_pixels,
            glyph_start,
            0,
            &write_offset);
        write_band_set(
            glyph->vertical_bands,
            glyph->curves,
            res.band_pixels,
            glyph_start,
            num_h_bands,
            &write_offset);
    }
    finalize_curve_pixels(&res);
    finalize_band_pixels(&res);
    return res;
}
