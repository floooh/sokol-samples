#pragma once
/*
    basisu_sokol.h -- C-API wrapper and sokol_gfx.h glue code for Basis Universal

    Include sokol_gfx.h before this file.
*/
#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

void sbasisu_setup(void);
void sbasisu_shutdown(void);
sg_image_desc sbasisu_transcode(const void* data, uint32_t num_bytes);
void sbasisu_free(const sg_image_desc* desc);

#if defined(__cplusplus)
} // extern "C"
#endif

