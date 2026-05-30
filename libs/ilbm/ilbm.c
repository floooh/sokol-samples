// simple IFF ILBM loader (https://en.wikipedia.org/wiki/ILBM)
#include "ilbm.h"
#include <memory.h>
#include <assert.h>
#include <stdlib.h>

static struct {
    const uint8_t* ptr;
    const uint8_t* end;
    int num_bitplanes;
    int16_t x_origin;
    int16_t y_origin;
    int16_t page_width;
    int16_t page_height;
    uint8_t mask;
    bool rle;           // current image body is runlength encoded
} state;

static uint32_t u32be(void) {
    if ((state.ptr + 4) <= state.end) {
        uint32_t b0 = *state.ptr++;
        uint32_t b1 = *state.ptr++;
        uint32_t b2 = *state.ptr++;
        uint32_t b3 = *state.ptr++;
        return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    } else {
        return 0;
    }
}

static uint32_t rgb_u32(void) {
    if ((state.ptr + 3) <= state.end) {
        uint32_t b0 = *state.ptr++;
        uint32_t b1 = *state.ptr++;
        uint32_t b2 = *state.ptr++;
        return 0xFF000000 | b0 | (b1 << 8) | (b2 << 16);
    } else {
        return 0;
    }
}

static uint16_t u16be(void) {
    if ((state.ptr + 2) <= state.end) {
        uint32_t b0 = *state.ptr++;
        uint32_t b1 = *state.ptr++;
        return (uint16_t)((b0 << 8) | b1);
    } else {
        return 0;
    }
}

static int16_t i16be(void) {
    return (int16_t)u16be();
}

static uint8_t u8(void) {
    if ((state.ptr + 1) <= state.end) {
        return *state.ptr++;
    } else {
        return 0;
    }
}

static bool load_bmhd(ilbm_t* ilbm) {
    assert(((uintptr_t)state.ptr & 1) == 0);
    const size_t chunk_size = 20;
    if (u32be() != chunk_size) return false;
    const uint8_t* start = state.ptr;
    ilbm->width = (int)u16be();
    ilbm->height = (int)u16be();
    if ((ilbm->width == 0) || (ilbm->height == 0)) return false;
    state.x_origin = i16be();
    state.y_origin = i16be();
    state.num_bitplanes = (int)u8();
    ilbm->num_colors = (1 << state.num_bitplanes);
    if ((ilbm->num_colors == 0) || (ilbm->num_colors > 256)) return false;
    state.mask = u8();
    if ((state.mask != 0) && (state.mask != 2)) return false;
    uint8_t compression = u8();
    if ((compression != 0) && (compression != 1)) return false;
    state.rle = compression == 1;
    u8();   // skip pad1
    u16be();    // skip transClr
    ilbm->x_aspect = (int)u8();
    ilbm->y_aspect = (int)u8();
    if ((ilbm->x_aspect == 0) || (ilbm->y_aspect == 0)) return false;
    ilbm->aspect_ratio = (float)(ilbm->width * ilbm->x_aspect) / (float)(ilbm->height * ilbm->y_aspect);
    state.page_width = i16be();
    state.page_height = i16be();
    assert(state.ptr == (start + chunk_size)); (void)start;

    // allocate pixel buffer
    ilbm->pixels.size = (size_t)(ilbm->width * ilbm->height);
    ilbm->pixels.ptr = calloc(ilbm->pixels.size, 1);

    return true;
}

static bool load_cmap(ilbm_t* ilbm) {
    assert(((uintptr_t)state.ptr & 1) == 0);
    assert(ilbm->num_colors > 0);
    const int chunk_size = (int)u32be();
    const uint8_t* start = state.ptr;
    const int num_colors = chunk_size / 3;
    if (num_colors > ilbm->num_colors) return false;
    int i = 0;
    for (; i < num_colors; i++) {
        ilbm->colors[i] = rgb_u32();
    }
    // CMAP chunk may have fewer colors than image bitplanes
    for (; i < ilbm->num_colors; i++) {
        ilbm->colors[i] = 0xFFFF00FF;
    }
    assert(state.ptr == (start + chunk_size)); (void)start;
    // may need to skip padding byte
    if (((uintptr_t)state.ptr & 1) == 1) {
        u8();
    }
    return true;
}

bool load_crng(ilbm_t* ilbm) {
    assert(((uintptr_t)state.ptr & 1) == 0);
    const size_t chunk_size = 8;
    if (u32be() != chunk_size) return false;
    if (ilbm->num_ranges >= ILBM_MAX_COLOR_RANGES) {
        state.ptr += chunk_size;
        return true;
    }
    int i = ilbm->num_ranges++;
    ilbm_color_range_t* rp = &ilbm->ranges[i];
    i16be(); // skip padding
    rp->rate = i16be();
    if (rp->rate > 0) {
        rp->rate_sec = ((1.0 / 60.0) * 16384.0) / (double)rp->rate;
    }
    int16_t flags = i16be();
    if (0 != (flags & 1)) {
        if (0 != (flags & 2)) {
            rp->cycle_backward = true;
        } else {
            rp->cycle_forward = true;
        }
    }
    rp->low = u8();
    rp->high = u8();
    return true;
}

bool load_body(ilbm_t* ilbm) {
    assert(((uintptr_t)state.ptr & 1) == 0);
    const int row_num_words = (ilbm->width + 15) / 16;
    const int row_num_bytes = row_num_words * 2;
    const int body_size = ilbm->height * state.num_bitplanes * row_num_bytes;

    uint32_t chunk_size = u32be();
    const uint8_t* chunk_end = state.ptr + chunk_size;

    // intermediate RLE decode buffer
    uint8_t* buf = calloc((size_t)body_size, 1);

    // RLE decode if needed
    if (state.rle) {
        uint8_t* dst = buf;
        uint8_t* dst_end = buf + body_size;
        while (dst < dst_end) {
            assert(state.ptr < chunk_end); (void)chunk_end;
            int8_t b = (int8_t)u8();
            if (b >= 0) {
                // literal run: copy b+1 bytes
                for (int i = 0; i <= b; i++) {
                    if (dst < dst_end) {
                        *dst++ = u8();
                    }
                }
            } else if (b != -128) {
                // repeat run: replicate next byte (1-b) times
                uint8_t val = u8();
                for (int i = 0; i < (1 - (int)b); i++) {
                    if (dst < dst_end) {
                        *dst++ = val;
                    }
                }
            }
        }
    } else {
        // otherwise just copy the body as-is into the RLE-decode buffer
        size_t copy_size = (size_t)body_size;
        if ((state.ptr + copy_size) > state.end) {
            copy_size = (size_t)(state.end - state.ptr);
        }
        assert(copy_size <= (size_t)body_size);
        memcpy(buf, state.ptr, copy_size);
        state.ptr += copy_size;
    }

    // deinterleave bitplanes into chunky pixels
    const uint8_t* src = buf;
    for (int y = 0; y < ilbm->height; y++) {
        for (int plane = 0; plane < state.num_bitplanes; plane++) {
            uint8_t* dst_row = (uint8_t*)ilbm->pixels.ptr + y * ilbm->width;
            int x = 0;
            for (int w = 0; w < row_num_words; w++) {
                uint16_t word = (uint16_t)((src[0] << 8) | src[1]);
                src += 2;
                for (int bitpos = 15; bitpos >= 0; bitpos--) {
                    if (x < ilbm->width) {
                        dst_row[x] |= ((word >> bitpos) & 1) << plane;
                    }
                    x += 1;
                }
            }
        }
    }
    free(buf);
    // skip padding if needed
    if (((uintptr_t)state.ptr & 1) == 1) {
        u8();
    }
    return true;
}

bool skip_chunk(void) {
    assert(((uintptr_t)state.ptr & 1) == 0);
    uint32_t chunk_size = u32be();
    if ((chunk_size & 1) == 1) {
        chunk_size += 1;
    }
    if ((state.ptr + chunk_size) > state.end) {
        return false;
    }
    state.ptr += chunk_size;
    return true;
}

#define ILBM_FOURCC(a, b, c, d) ((((uint32_t)a)<<24)|(((uint32_t)b)<<16)|(((uint32_t)c)<<8)|((uint32_t)d))

bool ilbm_load(ilbm_t* ilbm, ilbm_range_t data) {
    assert(ilbm && data.ptr && (data.size > 0));
    assert(ilbm->pixels.ptr == 0);

    state.ptr = data.ptr;
    state.end = (uint8_t*)data.ptr + data.size;

    if (u32be() != ILBM_FOURCC('F','O','R','M')) return false;
    if (u32be() > data.size) return false;
    if (u32be() != ILBM_FOURCC('I','L','B','M')) return false;
    while (state.ptr < state.end) {
        switch (u32be()) {
            case ILBM_FOURCC('B','M','H','D'):
                if (!load_bmhd(ilbm)) return false;
                break;
            case ILBM_FOURCC('C','M','A','P'):
                if (!load_cmap(ilbm)) return false;
                break;
            case ILBM_FOURCC('C','R','N','G'):
                if (!load_crng(ilbm)) return false;
                break;
            case ILBM_FOURCC('B','O','D','Y'):
                if (!load_body(ilbm)) return false;
                break;
            default:
                if (!skip_chunk()) return false;
                break;
        }
    }
    return true;
}

void ilbm_free(ilbm_t* ilbm) {
    assert(ilbm);
    if (ilbm->pixels.ptr) {
        free(ilbm->pixels.ptr);
    }
    memset(ilbm, 0, sizeof(ilbm_t));
}

bool ilbm_color_cycle(ilbm_t* ilbm, double frame_duration_sec) {
    bool needs_update = false;
    for (int i = 0; i < ilbm->num_ranges; i++) {
        ilbm_color_range_t* r = &ilbm->ranges[i];
        if (r->rate == 0) {
            continue;
        }
        if (!(r->cycle_forward || r->cycle_backward)) {
            continue;
        }
        if (r->low == r->high) {
            continue;
        }
        r->rate_accum += frame_duration_sec;
        while (r->rate_accum >= r->rate_sec) {
            needs_update = true;
            r->rate_accum -= r->rate_sec;
            if (r->cycle_forward) {
                uint32_t c = ilbm->colors[r->high];
                for (uint8_t ci = r->high; ci > r->low; ci--) {
                    ilbm->colors[ci] = ilbm->colors[ci-1];
                }
                ilbm->colors[r->low] = c;
            } else if (r->cycle_backward) {
                uint32_t c = ilbm->colors[r->low];
                for (uint8_t ci = r->low; ci < r->high; ci++) {
                    ilbm->colors[ci] = ilbm->colors[ci+1];
                }
                ilbm->colors[r->high] = c;
            }
        }
    }
    return needs_update;
}
