#pragma once
/*
    Platform entry helper code for WebGPU samples.
*/
#include <assert.h>
#include <webgpu/webgpu.h>
#include "sokol_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

// just the keys needed for the imgui sample
typedef enum {
    WGPU_KEY_INVALID = 0,
    WGPU_KEY_TAB,
    WGPU_KEY_LEFT,
    WGPU_KEY_RIGHT,
    WGPU_KEY_UP,
    WGPU_KEY_DOWN,
    WGPU_KEY_HOME,
    WGPU_KEY_END,
    WGPU_KEY_DELETE,
    WGPU_KEY_BACKSPACE,
    WGPU_KEY_ENTER,
    WGPU_KEY_ESCAPE,
} wgpu_keycode_t;

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
    bool no_depth_buffer;
    const char* title;
    wgpu_init_func init_cb;
    wgpu_frame_func frame_cb;
    wgpu_shutdown_func shutdown_cb;
} wgpu_desc_t;

typedef struct {
    wgpu_desc_t desc;
    bool async_setup_failed;
    bool async_setup_done;
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
    WGPUSurface surface;
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
sg_environment wgpu_environment(void);
sg_swapchain wgpu_swapchain(void);
void wgpu_key_down(wgpu_key_func fn);
void wgpu_key_up(wgpu_key_func fn);
void wgpu_char(wgpu_char_func fn);
void wgpu_mouse_btn_down(wgpu_mouse_btn_func fn);
void wgpu_mouse_btn_up(wgpu_mouse_btn_func fn);
void wgpu_mouse_pos(wgpu_mouse_pos_func fn);
void wgpu_mouse_wheel(wgpu_mouse_wheel_func fn);

// internal functions, don't call
void wgpu_swapchain_init(wgpu_state_t* state);
void wgpu_swapchain_discard(wgpu_state_t* state);
void wgpu_platform_start(wgpu_state_t* state);

#ifdef __cplusplus
} /* extern "C" */
#endif
