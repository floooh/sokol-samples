#pragma once
/*
    Quick'n'dirty app wrapper for OSX without using a .xib file.
*/
#include <TargetConditionals.h>
#include <wchar.h>
#include <stdbool.h>
#include "sokol_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* use CFBridgingRetain() to obtain the mtl_device ptr */
typedef void(*osx_init_func)(void);
typedef void(*osx_frame_func)(void);
typedef void(*osx_shutdown_func)(void);

typedef void(*osx_key_func)(int key);
typedef void(*osx_char_func)(wchar_t c);
typedef void(*osx_mouse_btn_func)(int btn);
typedef void(*osx_mouse_pos_func)(float x, float y);
typedef void(*osx_mouse_wheel_func)(float v);

/* entry function */
extern void osx_start(int w, int h, int sample_count, const char* title, osx_init_func, osx_frame_func, osx_shutdown_func);
/* return an initialized sg_context_desc struct */
sg_context_desc osx_get_context(void);
/* get width and height of drawable */
extern int osx_width();
extern int osx_height();
/* register key-down callback */
extern void osx_key_down(osx_key_func);
/* register key-up callback */
extern void osx_key_up(osx_key_func);
/* register character callback */
extern void osx_char(osx_char_func);
/* register mouse button down callback */
extern void osx_mouse_btn_down(osx_mouse_btn_func);
/* register mouse button up callback */
extern void osx_mouse_btn_up(osx_mouse_btn_func);
/* register mouse position callback */
extern void osx_mouse_pos(osx_mouse_pos_func);
/* register mouse wheel callback */
extern void osx_mouse_wheel(osx_mouse_wheel_func);

/* direct Objective-C access functions */
#if defined(__OBJC__)
#import <Metal/Metal.h>

extern id<MTLDevice> osx_mtl_device();
#endif

#ifdef __cplusplus
} // extern "C"
#endif
