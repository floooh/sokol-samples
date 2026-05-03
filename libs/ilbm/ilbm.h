// simple IFF ILBM loader (https://en.wikipedia.org/wiki/ILBM)
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void* ptr;
    size_t size;
} ilbm_range_t;

#define ILBM_MAX_COLOR_RANGES (16)
#define ILBM_MAX_BITPLANES (8)
#define ILBM_MAX_PALETTE_COLORS (1 << ILBM_MAX_BITPLANES)

typedef struct {
    int rate;       // 60 steps per second = (1<<14), 30 steps per second = (1<<13) etc...
    bool cycle_forward;
    bool cycle_backward;
    int low;        // low index in color palette
    int high;       // high index in color palette
    double rate_sec;    // cycle rate in seconds
    double rate_accum;  // curren rate acculumator
} ilbm_color_range_t;

typedef struct {
    int width;
    int height;
    int x_aspect;
    int y_aspect;
    int num_ranges;
    int num_colors;
    ilbm_color_range_t ranges[ILBM_MAX_COLOR_RANGES];
    uint32_t colors[ILBM_MAX_PALETTE_COLORS];
    ilbm_range_t pixels;
} ilbm_t;

bool ilbm_load(ilbm_t* ilbm, ilbm_range_t data);
void ilbm_free(ilbm_t* ilbm);
bool ilbm_color_cycle(ilbm_t* ilbm, double frame_duration_sec);
