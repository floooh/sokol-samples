//------------------------------------------------------------------------------
//  objcpp-compile-test.mm
//  Just check whether (most of) the sokol headers also comppile as C++.
//------------------------------------------------------------------------------
#if !defined(__APPLE__)
#error "This is for Apple platforms only"
#endif
#define SOKOL_METAL
#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_args.h"
#include "sokol_audio.h"
#include "sokol_gfx.h"
#include "sokol_time.h"

static sapp_desc desc;

sapp_desc sokol_main(int argc, char* argv[]) {
    /* just interested whether the compilation worked, so force-exit here */
    exit(0);
    return desc;
}
