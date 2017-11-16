//------------------------------------------------------------------------------
//  sokol_gfx.m
//
//  When using the Metal backend, the implementation source must be
//  Objective-C (.m or .mm), but we want the samples to be in C. Thus
//  move the sokol_gfx implementation into it's own .m file.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#if defined(SOKOL_USE_MACOS)
#define SOKOL_METAL_MACOS
#else
#define SOKOL_METAL_IOS
#endif
#include "sokol_gfx.h"

