/*
    wgpu_entry.cc

    C wrappers for libdawn C++ functions
*/
#include "webgpu/webgpu_cpp.h"
#include "webgpu/webgpu_glfw.h"
#include "wgpu_entry.h"

WGPUSurface wgpu_glfw_create_surface_for_window(WGPUInstance instance, void* glfw_window) {
    wgpuInstanceReference(instance);
    const auto cppInstance = wgpu::Instance::Acquire(instance);
    const auto cpp_surface = wgpu::glfw::CreateSurfaceForWindow(instance, (GLFWwindow*)glfw_window);
    WGPUSurface surface = cpp_surface.Get();
    wgpuSurfaceReference(surface);
    return surface;
}
