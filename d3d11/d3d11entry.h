#pragma once
/*
    app wrapper for D3D11 samples
*/
#ifndef UNICODE
#define UNICODE
#endif
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
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
extern ID3D11Device* d3d11_device();
/* get pointer to D3D11DeviceContext */
extern ID3D11DeviceContext* d3d11_device_context();
/* get current framebuffer width */
extern int d3d11_width();
/* get current framebuffer height */
extern int d3d11_height();

#ifdef __cplusplus
} // extern "C"
#endif