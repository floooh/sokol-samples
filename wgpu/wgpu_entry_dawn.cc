//------------------------------------------------------------------------------
//  wgpu_entry_dawn.cc
//
//  Dawn-specific wgpu entry code.
//
// NOTE: we could use https://github.com/eliemichel/glfw3webgpu/ and avoid C++.
//------------------------------------------------------------------------------
#include "GLFW/glfw3.h"
#include "webgpu/webgpu_cpp.h"
#include "webgpu/webgpu_glfw.h"
#include "wgpu_entry.h"
#include "stdio.h"

static WGPUSurface glfw_create_surface_for_window(WGPUInstance instance, void* glfw_window) {
    wgpuInstanceAddRef(instance);
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

static void glfw_resize_cb(GLFWwindow* window, int width, int height) {
    wgpu_state_t* state = (wgpu_state_t*) glfwGetWindowUserPointer(window);
    state->width = width;
    state->height = height;
    wgpu_swapchain_resized(state);
}

static void uncaptured_error_cb(const WGPUDevice* dev, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)dev; (void)userdata1; (void)userdata2;
    if (type != WGPUErrorType_NoError) {
        printf("UNCAPTURED ERROR: %s\n", message.data);
    }
}

static void device_lost_cb(const WGPUDevice* dev, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)dev; (void)reason; (void)userdata1; (void)userdata2;
    printf("DEVICE LOST: %s\n", message.data);
}

static void error_scope_cb(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)status; (void)userdata1; (void)userdata2;
    if (type != WGPUErrorType_NoError) {
        printf("ERROR: %s\n", message.data);
    }
}

static void logging_cb(WGPULoggingType type, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)type; (void)userdata1; (void)userdata2;
    printf("LOG: %s\n", message.data);
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)status; (void)message; (void)userdata2;
    wgpu_state_t* state = (wgpu_state_t*) userdata1;
    state->device = device;
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2) {
    (void)message; (void)userdata2;
    wgpu_state_t* state = (wgpu_state_t*) userdata1;
    if (status != WGPURequestAdapterStatus_Success) {
        printf("wgpuInstanceRequestAdapter failed!\n");
        exit(10);
    }
    state->adapter = adapter;
}

static void request_adapter(wgpu_state_t* state) {
    WGPUFuture future = wgpuInstanceRequestAdapter(state->instance, 0, {
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = request_adapter_cb,
        .userdata1 = state,
    });
    WGPUFutureWaitInfo future_info = { .future = future };
    WGPUWaitStatus res = wgpuInstanceWaitAny(state->instance, 1, &future_info, UINT64_MAX);
    assert(res == WGPUWaitStatus_Success);
}

static void request_device(wgpu_state_t* state) {
    WGPUFeatureName required_features[1] = {
        WGPUFeatureName_Depth32FloatStencil8
    };
    WGPUDeviceDescriptor dev_desc = {
        .requiredFeatureCount = 1,
        .requiredFeatures = required_features,
        .deviceLostCallbackInfo = {
            .mode = WGPUCallbackMode_AllowProcessEvents,
            .callback = device_lost_cb,
        },
        .uncapturedErrorCallbackInfo = {
            .callback = uncaptured_error_cb,
        },
    };
    WGPUFuture future = wgpuAdapterRequestDevice(
        state->adapter,
        &dev_desc,
        {
            .mode = WGPUCallbackMode_WaitAnyOnly,
            .callback = request_device_cb,
            .userdata1 = state,
        });
    WGPUFutureWaitInfo future_info = { .future = future };
    WGPUWaitStatus res = wgpuInstanceWaitAny(state->instance, 1, &future_info, UINT64_MAX);
    assert(res == WGPUWaitStatus_Success);
    assert(state->device);
}

void wgpu_platform_start(wgpu_state_t* state) {
    assert(state->instance == 0);

    WGPUInstanceDescriptor inst_desc = {
        .capabilities = {
            .timedWaitAnyEnable = true,
        }
    };
    state->instance = wgpuCreateInstance(&inst_desc);
    assert(state->instance);
    request_adapter(state);
    request_device(state);

    wgpuDeviceSetLoggingCallback(state->device, { .callback = logging_cb });
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
    glfwSetWindowSizeCallback(window, glfw_resize_cb);

    state->surface = glfw_create_surface_for_window(state->instance, window);
    assert(state->surface);
    WGPUSurfaceCapabilities surf_caps;
    wgpuSurfaceGetCapabilities(state->surface, state->adapter, &surf_caps);
    state->render_format = surf_caps.formats[0];

    wgpu_swapchain_init(state);
    state->desc.init_cb();
    wgpuDevicePopErrorScope(state->device, { .mode = WGPUCallbackMode_AllowProcessEvents, .callback = error_scope_cb });
    wgpuInstanceProcessEvents(state->instance);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        wgpuDevicePushErrorScope(state->device, WGPUErrorFilter_Validation);
        state->swapchain_view = wgpu_swapchain_next(state);
        if (state->swapchain_view) {
            state->desc.frame_cb();
            wgpuTextureViewRelease(state->swapchain_view);
            state->swapchain_view = 0;
            wgpuSurfacePresent(state->surface);
        }
        wgpuDevicePopErrorScope(state->device, { .mode = WGPUCallbackMode_AllowProcessEvents, .callback = error_scope_cb });
        wgpuInstanceProcessEvents(state->instance);
    }
    state->desc.shutdown_cb();
    wgpu_swapchain_discard(state);
    wgpuDeviceRelease(state->device);
    wgpuAdapterRelease(state->adapter);
}
