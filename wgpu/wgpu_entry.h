#pragma once
/*
    Platform entry helper code for WebGPU samples.
*/
#include <webgpu/webgpu.h>
#include "sokol_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wgpu_init_func)(void);
typedef void (*wgpu_frame_func)(void);
typedef void (*wgpu_shutdown_func)(void);
typedef void (*wgpu_key_func)(int key);
typedef void (*wgpu_char_func)(uint32_t c);
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
} wgpu_desc_t;

typedef struct {
    wgpu_desc_t desc;
    int width;
    int height;
    wgpu_key_func key_down_cb;
    wgpu_key_func key_up_cb;
    wgpu_char_func char_cb;
    wgpu_mouse_btn_func mouse_btn_down_cb;
    wgpu_mouse_btn_func mouse_btn_up_cb;
    wgpu_mouse_pos_func mouse_pos_cb;
    wgpu_mouse_wheel_func mouse_wheel_cb;
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
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
void wgpu_key_down(wgpu_key_func fn);
void wgpu_key_up(wgpu_key_func fn);
void wgpu_char(wgpu_char_func fn);
void wgpu_mouse_btn_down(wgpu_mouse_btn_func fn);
void wgpu_mouse_btn_up(wgpu_mouse_btn_func fn);
void wgpu_mouse_pos(wgpu_mouse_pos_func fn);
void wgpu_mouse_wheel(wgpu_mouse_wheel_func fn);

// internals, don't use
WGPUSurface wgpu_glfw_create_surface_for_window(WGPUInstance instance, void* glfw_window);

#ifdef __cplusplus
} /* extern "C" */
#endif
