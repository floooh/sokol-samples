#pragma once
/*
    The typical debug UI overlay useful for most sokol-app samples
*/
#if defined(USE_DBG_UI)
#include "sokol_app.h"
#if defined(__cplusplus)
extern "C" {
#endif
extern void __dbgui_setup(int sample_count);
extern void __dbgui_shutdown(void);
extern void __dbgui_draw(void);
extern void __dbgui_event(const sapp_event* e);
#if defined(__cplusplus)
} // extern "C"
#endif
#else
#define __dbgui_setup(x)
#define __dbgui_shutdown()
#define __dbgui_draw()
#define __dbgui_event(x) (void)(0)
#endif
