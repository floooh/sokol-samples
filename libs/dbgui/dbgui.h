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
extern bool __dbgui_event_with_retval(const sapp_event* e);
#if defined(__cplusplus)
} // extern "C"
#endif
#else
static inline void __dbgui_setup(int sample_count) { (void)(sample_count); }
static inline void __dbgui_shutdown(void) { }
static inline void __dbgui_draw(void) { }
static inline void __dbgui_event(const sapp_event* e) { (void)(e); }
static inline bool __dbgui_event_with_retval(const sapp_event* e) { (void)(e); return false; }
#endif
