// simple IFF ILBM loader (https://en.wikipedia.org/wiki/ILBM)
#include "ilbm.h"
#include <memory.h>
#include <assert.h>
#include <stdlib.h>

static struct {
    const uint8_t* ptr;
    const uint8_t* end;
    bool rle;           // current image body is runlength encoded
} state;

static uint32_t u32be(void) {
    if ((state.ptr + 4) <= state.end) {
        return (*state.ptr++ << 24) | (*state.ptr++ << 16) | (*state.ptr++ << 8) | *state.ptr++;
    } else {
        return 0;
    }
}

static uint32_t rgb_u32(void) {
    if ((state.ptr + 3) <= state.end) {
        return 0xFF000000 | (*state.ptr++ << 16) | (*state.ptr++ << 8) | *state.ptr++;
    } else {
        return 0;
    }
}

static uint16_t u16be(void) {
    if ((state.ptr + 2) <= state.end) {
        return (*state.ptr++ << 8) | *state.ptr++;
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
    i16be();    // skip xOrigin
    i16be();    // skip yOrigin
    ilbm->num_colors = (1 << u8());
    if ((ilbm->num_colors == 0) || (ilbm->num_colors > 256)) return false;
    u8();   // skip mask
    uint8_t compression = u8();
    if ((compression != 0) && (compression != 1)) return false;
    state.rle = compression == 1;
    u8();   // skip pad1
    u16be();    // skip transClr
    ilbm->x_aspect = (int)u8();
    ilbm->y_aspect = (int)u8();
    if ((ilbm->x_aspect == 0) || (ilbm->y_aspect == 0)) return false;
    i16be(); // skip pageWidth
    i16be(); // skip pageHeight
    assert(state.ptr == (start + chunk_size));

    // allocate pixel buffer
    ilbm->pixels.size = (size_t)(ilbm->width * ilbm->height);
    ilbm->pixels.ptr = malloc(ilbm->pixels.size);

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
    assert(state.ptr == (start + chunk_size));
    // may need to skip padding byte
    if (((uintptr_t)state.ptr & 1) == 1) {
        u8();
    }
    return true;
}

bool load_crng(ilbm_t* ilbm) {
    return false;
}

bool load_body(ilbm_t* ilbm) {
    return false;
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

bool ilbm_load(ilbm_t* ilbm, ilbm_range_t data) {
    assert(ilbm && data.ptr && (data.size > 0));
    assert(ilbm->pixels.ptr == 0);

    state.ptr = data.ptr;
    state.end = data.ptr + data.size;

    if (u32be() != 'FORM') return false;
    if (u32be() > data.size) return false;
    if (u32be() != 'ILBM') return false;
    while (state.ptr < state.end) {
        switch (u32be()) {
            case 'BMHD':
                if (!load_bmhd(ilbm)) return false;
                break;
            case 'CMAP':
                if (!load_cmap(ilbm)) return false;
                break;
            //case 'CRNG':
            //    if (!load_crng(ilbm)) return false;
            //    break;
            //case 'BODY':
            //    if (!load_body(ilbm)) return false;
            //    break;
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
