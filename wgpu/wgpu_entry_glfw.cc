#include "wgpu_entry.h"
#include "GLFW/glfw3.h"
#include "webgpu/webgpu_cpp.h"
#include "webgpu/webgpu_glfw.h"

#include <stdio.h>
#include <assert.h>

wgpu::Instance instance;
wgpu::Device device;
wgpu::SwapChain swapchain;

static void get_device(void) {
    instance.RequestAdapter(
        nullptr,
        [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter, const char* message, void* userdata) {
            (void)message;
            (void)userdata;
            if (status != WGPURequestAdapterStatus_Success) {
                printf("RequestAdapter failed!\n");
                exit(10);
            }
            wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
            adapter.RequestDevice(
                nullptr,
                [](WGPURequestDeviceStatus status, WGPUDevice cDevice, const char* message, void* userdata) {
                    (void)status;
                    (void)message;
                    (void)userdata;
                    device = wgpu::Device::Acquire(cDevice);
                },
                nullptr
            );
        },
        nullptr
    );
}

static void setup_swapchain(GLFWwindow* window, int width, int height) {
    wgpu::Surface surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
    wgpu::SwapChainDescriptor scDesc = {
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = wgpu::TextureFormat::BGRA8Unorm,
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .presentMode = wgpu::PresentMode::Fifo
    };
    swapchain = device.CreateSwapChain(surface, &scDesc);
}

void wgpu_platform_start(const wgpu_desc_t* desc) {
    instance = wgpu::CreateInstance();
    get_device();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(desc->width, desc->height, desc->title, nullptr, nullptr);
    setup_swapchain(window, desc->width, desc->height);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        swapchain.Present();
    }
}
