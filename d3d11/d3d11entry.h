#pragma once
/*
    app wrapper for D3D11 samples
*/
#include <windows.h>
#include <d3d11.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* initialize (window, d3d11 device, swapchain, ...) */
extern void d3d11_init(int w, int h, int sample_count, const char* title);
/* shutdown function */
extern void d3d11_shutdown();
/* present the current frame */
extern void d3d11_present();
/* handle messages, return true if app should quit */
extern bool d3d11_process_events();

#ifdef __cplusplus
} // extern "C"
#endif