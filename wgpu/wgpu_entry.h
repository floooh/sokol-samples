#pragma once
/*
    Platform entry helper code for WebGPU samples.
*/
#if defined(__EMSCRIPTEN__)
#include <webgpu/webgpu.h>
#else
#include <dawn/webgpu.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wgpu_init_func)(void);
typedef void (*wgpu_frame_func)(void);
typedef void (*wgpu_shutdown_func)(void);
typedef void (*wgpu_key_func)(int key);
typedef void (*wgpu_char_func)(wchar_t c);
typedef void (*wgpu_mouse_btn_func)(int btn);
typedef void (*wgpu_mouse_pos_func)(float x, float y);
typedef void (*wgpu_mouse_wheel_func)(float v);

typedef struct {
    int width;
    int height;
    int sample_count;
    const char* title;
    wgpu_init_func init_cb;
    wgpu_frame_func frame_cb;
    wgpu_shutdown_func shutdown_cb;
    wgpu_key_func key_down_cb;
    wgpu_key_func key_up_cb;
    wgpu_char_func char_cb;
    wgpu_mouse_btn_func mouse_btn_down_cb;
    wgpu_mouse_btn_func mouse_btn_up_cb;
    wgpu_mouse_pos_func mouse_pos_cb;
    wgpu_mouse_wheel_func mouse_wheel_cb;
} wgpu_desc_t;

typedef struct {
    wgpu_desc_t desc;
    int width;
    int height;
    WGPUDevice dev;
    WGPUSwapChain swapchain;
    WGPUTextureFormat render_format;
    WGPUTexture msaa_tex;
    WGPUTexture depth_stencil_tex;
    WGPUTextureView swapchain_view;
    WGPUTextureView msaa_view;
    WGPUTextureView depth_stencil_view;
} wgpu_state_t;

void wgpu_start(const wgpu_desc_t* desc);
int wgpu_width(void);
int wgpu_height(void);
sg_context_desc wgpu_get_context(void);

/* internals, don't use */
extern wgpu_state_t wgpu_state;
void wgpu_platform_start(const wgpu_desc_t* desc);
void wgpu_swapchain_init(void);
void wgpu_swapchain_next_frame(void);
void wgpu_swapchain_discard(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
