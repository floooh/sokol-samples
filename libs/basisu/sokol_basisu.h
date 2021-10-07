#pragma once
/*
    basisu_sokol.h -- C-API wrapper and sokol_gfx.h glue code for Basis Universal

    Include sokol_gfx.h before this file.
*/
#include <stdint.h>
#include <stdbool.h>
#include "sokol_gfx.h"

#if defined(__cplusplus)
extern "C" {
#endif

void sbasisu_setup(void);
void sbasisu_shutdown(void);

// all in one image creation function
sg_image sbasisu_make_image(sg_range basisu_data);

// optional for finer control
sg_image_desc sbasisu_transcode(sg_range basisu_data);
void sbasisu_free(const sg_image_desc* desc);

// query supported pixel format
sg_pixel_format sbasisu_pixelformat(bool has_alpha);

#if defined(__cplusplus)
} // extern "C"
#endif

