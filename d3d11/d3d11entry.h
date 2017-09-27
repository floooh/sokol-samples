#pragma once
/*
    app wrapper for D3D11 samples
*/
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* initialize (window, d3d11 device, swapchain, ...) */
extern void d3d11_init(int w, int h, int sample_count, const wchar_t* title);
/* shutdown function */
extern void d3d11_shutdown();
/* present the current frame */
extern void d3d11_present();
/* handle messages, return true if app should quit */
extern bool d3d11_process_events();

/* get pointer to D3D11Device */
extern const void* d3d11_device(void);
/* get pointer to D3D11DeviceContext */
extern const void* d3d11_device_context(void);
/* get pointer to current render-target-view object */
extern const void* d3d11_render_target_view(void);
/* get pointer to current depth-stencil-view object */
extern const void* d3d11_depth_stencil_view(void);
/* get current framebuffer width */
extern int d3d11_width();
/* get current framebuffer height */
extern int d3d11_height();

#ifdef __cplusplus
} // extern "C"
#endif