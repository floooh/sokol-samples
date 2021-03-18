#include "sokol_app.h"
#include "sokol_gfx.h"

// include nuklear.h before the sokol_nuklear.h implementation
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_IMPLEMENTATION

// NOTE: in real-world code it makes sense to include the nuklear.h implementation
// in a separate .c file and do all the "warning hygiene" there.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#if defined(_MSC_VER)
#pragma warning(disable:4996)   // sprintf,fopen,localtime: This function or variable may be unsafe
#pragma warning(disable:4127)   // conditional expression is constant
#pragma warning(disable:4100)   // 'type': unreferenced formal parameter
#pragma warning(disable:4701)   // potentially uninitialized local variable 'num_len'
#endif
#include "nuklear/nuklear.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#define SOKOL_IMPL
#include "sokol_nuklear.h"

void use_nuklear_impl(void) {
    snk_setup(&(snk_desc_t){0});
}
