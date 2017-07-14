//------------------------------------------------------------------------------
//  clear-glfw.cc
//------------------------------------------------------------------------------
#define SOKOL_IMPLEMENTATION
#define SOKOL_GFX_USE_GL
#include "sokol_gfx.h"
#include <stdio.h>

int main() {
    sg_setup_desc setup_desc;
    sg_init_setup_desc(&setup_desc);
    sg_setup(&setup_desc);

    sg_push_label(sg_gen_label());

    sg_label l = sg_pop_label();

    printf("hello world!\n");
    sg_discard();
}
