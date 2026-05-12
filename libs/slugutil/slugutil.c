#include "slugutil.h"
#include <assert.h>

bool slug_load_font(slug_font_t* font, float pixel_size, const slug_range_t* data) {
    assert(font);
    assert(pixel_size > 0.0f);
    assert(data && data->ptr && data->size > 0);
    *font = (slug_font_t){0};
    // FIXME
    return false;
}

void slug_unload_font(slug_font_t* font) {
    // FIXME
    *font = (slug_font_t){0};
}
