#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_gl.h"

#define FONTSTASH_IMPLEMENTATION
#if defined(_MSC_VER )
#pragma warning(disable:4996)   // strncpy use in fontstash.h
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <stdlib.h>     // malloc/free
#include "fontstash/fontstash.h"
#define SOKOL_IMPL
#include "sokol_fontstash.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sfons_create(0, 0, 0);
    return (sapp_desc){0};
}
