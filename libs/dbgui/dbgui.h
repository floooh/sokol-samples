#pragma once
/*
    The typical debug UI overlay useful for most sokol-app samples
*/
#if defined(USE_DBG_UI)
#include "sokol_app.h"
#if defined(__cplusplus)
extern "C" {
#endif
extern void _dbgui_setup(void);
extern void _dbgui_shutdown(void);
extern void _dbgui_update(void);
extern void _dbgui_draw(void);
extern void _dbgui_event(const sapp_event* e);
extern bool _dbgui_event_with_retval(const sapp_event* e);
#if defined(__cplusplus)
} // extern "C"
#endif
#else
static inline void _dbgui_setup(void) { }
static inline void _dbgui_shutdown(void) { }
static inline void _dbgui_update(void) { }
static inline void _dbgui_draw(void) { }
static inline void _dbgui_event(const sapp_event* e) { (void)(e); }
static inline bool _dbgui_event_with_retval(const sapp_event* e) { (void)(e); return false; }
#endif
