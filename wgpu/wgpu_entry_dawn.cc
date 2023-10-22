//------------------------------------------------------------------------------
//  wgpu_entry_dawn.c
//
//  Dawn-specific wgpu entry code.
//------------------------------------------------------------------------------
#include "GLFW/glfw3.h"
#include "webgpu/webgpu_cpp.h"
#include "webgpu/webgpu_glfw.h"
#include "wgpu_entry.h"

static WGPUSurface glfw_create_surface_for_window(WGPUInstance instance, void* glfw_window) {
    wgpuInstanceReference(instance);
    const auto cppInstance = wgpu::Instance::Acquire(instance);
    WGPUSurface surface = wgpu::glfw::CreateSurfaceForWindow(instance, (GLFWwindow*)glfw_window).MoveToCHandle();
    return surface;
}

static void glfw_key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    wgpu_state_t* state = (wgpu_state_t*) glfwGetWindowUserPointer(window);
    wgpu_keycode_t wgpu_key = WGPU_KEY_INVALID;
    switch (key) {
        case GLFW_KEY_TAB:          wgpu_key = WGPU_KEY_TAB; break;
        case GLFW_KEY_LEFT:         wgpu_key = WGPU_KEY_LEFT; break;
        case GLFW_KEY_RIGHT:        wgpu_key = WGPU_KEY_RIGHT; break;
        case GLFW_KEY_UP:           wgpu_key = WGPU_KEY_UP; break;
        case GLFW_KEY_DOWN:         wgpu_key = WGPU_KEY_DOWN; break;
        case GLFW_KEY_HOME:         wgpu_key = WGPU_KEY_HOME; break;
        case GLFW_KEY_END:          wgpu_key = WGPU_KEY_END; break;
        case GLFW_KEY_DELETE:       wgpu_key = WGPU_KEY_DELETE; break;
        case GLFW_KEY_BACKSPACE:    wgpu_key = WGPU_KEY_BACKSPACE; break;
        case GLFW_KEY_ENTER:        wgpu_key = WGPU_KEY_ENTER; break;
        case GLFW_KEY_ESCAPE:       wgpu_key = WGPU_KEY_ESCAPE; break;
    }
    if ((action == GLFW_PRESS) && state->key_down_cb) {
        state->key_down_cb(wgpu_key);
    } else if ((action == GLFW_RELEASE) && state->key_up_cb) {
        state->key_up_cb(wgpu_key);
    }
}

static void glfw_char_cb(GLFWwindow* window, unsigned int chr) {
    wgpu_state_t* state = (wgpu_state_t*) glfwGetWindowUserPointer(window);
    if (state->char_cb) {
        state->char_cb(chr);
    }
}

static void glfw_mousebutton_cb(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    wgpu_state_t* state = (wgpu_state_t*) glfwGetWindowUserPointer(window);
    if ((action == GLFW_PRESS) && state->mouse_btn_down_cb) {
        state->mouse_btn_down_cb(button);
    } else if ((action == GLFW_RELEASE) && state->mouse_btn_up_cb) {
        state->mouse_btn_up_cb(button);
    }
}

static void glfw_cursorpos_cb(GLFWwindow* window, double xpos, double ypos) {
    wgpu_state_t* state = (wgpu_state_t*) glfwGetWindowUserPointer(window);
    if (state->mouse_pos_cb) {
        state->mouse_pos_cb((float)xpos, (float)ypos);
    }
}

static void glfw_scroll_cb(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    wgpu_state_t* state = (wgpu_state_t*) glfwGetWindowUserPointer(window);
    if (state->mouse_wheel_cb) {
        state->mouse_wheel_cb((float)yoffset);
    }
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, const char* msg, void* userdata) {
    (void)status; (void)msg; (void)userdata;
    wgpu_state_t* state = (wgpu_state_t*) userdata;
    state->device = device;
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* msg, void* userdata) {
    (void)msg;
    wgpu_state_t* state = (wgpu_state_t*) userdata;
    if (status != WGPURequestAdapterStatus_Success) {
        printf("wgpuInstanceRequestAdapter failed!\n");
        exit(10);
    }
    state->adapter = adapter;
    WGPUFeatureName requiredFeatures[1] = {
        WGPUFeatureName_Depth32FloatStencil8
    };
    WGPUDeviceDescriptor dev_desc = {
        .requiredFeaturesCount = 1,
        .requiredFeatures = requiredFeatures,
    };
    wgpuAdapterRequestDevice(adapter, &dev_desc, request_device_cb, userdata);
    assert(0 != state->device);
}

static void error_cb(WGPUErrorType type, const char* message, void* userdata) {
    (void)type; (void)userdata;
    if (type != WGPUErrorType_NoError) {
        printf("ERROR: %s\n", message);
    }
}

static void logging_cb(WGPULoggingType type, const char* message, void* userdata) {
    (void)type; (void)userdata;
    printf("LOG: %s\n", message);
}

void wgpu_platform_start(wgpu_state_t* state) {
    assert(state->instance == 0);

    state->instance = wgpuCreateInstance(0);
    assert(state->instance);
    wgpuInstanceRequestAdapter(state->instance, 0, request_adapter_cb, state);
    assert(state->device);

    wgpuDeviceSetUncapturedErrorCallback(state->device, error_cb, 0);
    wgpuDeviceSetLoggingCallback(state->device, logging_cb, 0);
    wgpuDeviceSetDeviceLostCallback(state->device, 0, 0);
    wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(state->width, state->height, state->desc.title, 0, 0);
    glfwSetWindowUserPointer(window, state);
    glfwSetKeyCallback(window, glfw_key_cb);
    glfwSetCharCallback(window, glfw_char_cb);
    glfwSetMouseButtonCallback(window, glfw_mousebutton_cb);
    glfwSetCursorPosCallback(window, glfw_cursorpos_cb);
    glfwSetScrollCallback(window, glfw_scroll_cb);

    state->surface = glfw_create_surface_for_window(state->instance, window);
    assert(state->surface);
    // FIXME: Dawn doesn't support wgpuSurfaceGetPreferredFormat?
    state->render_format = WGPUTextureFormat_BGRA8Unorm;

    wgpu_swapchain_init(state);
    state->desc.init_cb();
    wgpuDevicePopErrorScope(state->device, error_cb, 0);
    wgpuInstanceProcessEvents(state->instance);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);
        state->swapchain_view = wgpuSwapChainGetCurrentTextureView(state->swapchain);
        state->desc.frame_cb();
        wgpuSwapChainPresent(state->swapchain);
        wgpuTextureViewRelease(state->swapchain_view);
        state->swapchain_view = 0;
        wgpuDevicePopErrorScope(state->device, error_cb, 0);
        wgpuInstanceProcessEvents(state->instance);
    }
    state->desc.shutdown_cb();
    wgpu_swapchain_discard(state);
    wgpuDeviceRelease(state->device);
    wgpuAdapterRelease(state->adapter);
    wgpuInstanceRelease(state->instance);
}
