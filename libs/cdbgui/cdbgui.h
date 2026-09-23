#pragma once
/*
    The typical debug UI overlay useful for most sokol-app samples
*/
#if defined(USE_DBG_UI)
#include "sokol_app.h"
extern void _cdbgui_setup(void);
extern void _cdbgui_shutdown(void);
extern void _cdbgui_update(void);
extern void _cdbgui_draw(void);
extern void _cdbgui_event(const sapp_event* e);
#else
static inline void _cdbgui_setup(void) { }
static inline void _cdbgui_shutdown(void) { }
static inline void _cdbgui_update(void) { }
static inline void _cdbgui_draw(void) { }
static inline void _cdbgui_event(const sapp_event* e) { (void)(e); }
#endif
