#pragma once
/*
    app wrapper for D3D11 samples
*/
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxguid.lib")

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

#ifdef __cplusplus
} // extern "C"
#endif