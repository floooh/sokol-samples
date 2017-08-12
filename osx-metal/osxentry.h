#pragma once
/*
    Quick'n'dirty app wrapper for OSX without using a .xib file.
*/

/* use CFBridgingRetain() to obtain the mtl_device ptr */
typedef void(*osx_init_func)(const void* mtl_device);
typedef void(*osx_frame_func)(void);
typedef void(*osx_shutdown_func)(void);

/* entry function */
extern void osx_start(int w, int h, int sample_count, const char* title, osx_init_func, osx_frame_func, osx_shutdown_func);
/* CFBridgingRetain([MTKView currentRenderPassDescriptor]) */
extern const void* osx_mtk_get_render_pass_descriptor();
/* CFBridgingRetain([MTKView currentDrawable]) */
extern const void* osx_mtk_get_drawable();
/* get width and height of drawable */
extern int osx_width();
extern int osx_height();

