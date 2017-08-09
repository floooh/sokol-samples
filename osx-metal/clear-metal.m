//------------------------------------------------------------------------------
//  clear-metal.m
//------------------------------------------------------------------------------
#include "osxentry.h"
#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol_gfx.h"

void init(const void* mtl_device);
void frame();
void shutdown();

int main() {
    osx_start(640, 480, init, frame, shutdown);
    return 0;
}

void init(const void* mtl_device) {
    sg_desc desc = { 0 };
    desc.mtl_device = mtl_device;
    sg_setup(&desc);

}

void frame() {

}

void shutdown() {
    sg_shutdown();
}

