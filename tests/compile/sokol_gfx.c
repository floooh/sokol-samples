#define SOKOL_IMPL
// FIXME: get rid of sokol_app.h dependency!
#include "sokol_app.h"
#include "sokol_gfx.h"

void use_gfx_impl(void) {
    sg_setup(&(sg_desc){0});
}
