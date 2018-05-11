//------------------------------------------------------------------------------
//  clear-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"

void sokol_enter() {
    sapp_setup(&(sapp_desc){
        .width = 400,
        .height = 300,
        .window_title = "Clear (sapp)"
    });
}

void sokol_frame() {

}

int sokol_exit() {
    sapp_shutdown();
    return 0;
}