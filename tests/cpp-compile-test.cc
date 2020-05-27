//------------------------------------------------------------------------------
//  cpp-compile-test.cc
//  Just check whether (most of) the sokol headers also comppile as C++.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_WIN32_FORCE_MAIN
#ifdef _MSC_VER
#pragma warning(disable:4702)   /* unreachable code */
/* don't complain about unused variables in MSVC at /W4 when assert() is a NOP */
#if defined(NDEBUG)
#define SOKOL_ASSERT(x) ((void)(x))
#endif
#endif
#include "sokol_app.h"
#include "sokol_args.h"
#include "sokol_audio.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_fetch.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"

static sapp_desc desc;

sapp_desc sokol_main(int /*argc*/, char* /*argv*/[]) {
    /* just interested whether the compilation worked, so force-exit here */
    exit(0);
    return desc;
}
