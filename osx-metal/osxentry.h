#pragma once
/*
    Quick'n'dirty app wrapper for OSX without using a .xib file.
*/

// use CFBridgingRetain() to obtain the mtl_device ptr
typedef void(*osx_init_func)(const void* mtl_device);
typedef void(*osx_frame_func)(void);
typedef void(*osx_shutdown_func)(void);

extern void osx_start(int w, int h, osx_init_func, osx_frame_func, osx_shutdown_func);
