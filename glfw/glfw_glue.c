#include "sokol_gfx.h"
#include "glfw_glue.h"

static int _sample_count;
GLFWwindow* _window;

void glfw_init(const char* title, int width, int height, int sample_count) {
    _sample_count = sample_count;
    glfwInit();
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, 0);
    glfwWindowHint(GLFW_SAMPLES, (sample_count == 1) ? 0 : sample_count);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    _window = glfwCreateWindow(width, height, title, 0, 0);
    glfwMakeContextCurrent(_window);
    glfwSwapInterval(1);
}

GLFWwindow* glfw_window(void) {
    return _window;
}

int glfw_width(void) {
    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    return width;
}

int glfw_height(void) {
    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    return height;
}

sg_environment glfw_environment(void) {
    return (sg_environment) {
        .defaults = {
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .sample_count = _sample_count,
        },
    };
}

sg_swapchain glfw_swapchain(void) {
    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    return (sg_swapchain) {
        .width = width,
        .height = height,
        .sample_count = _sample_count,
        .color_format = SG_PIXELFORMAT_RGBA8,
        .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .gl = {
            // we just assume here that the GL framebuffer is always 0
            .framebuffer = 0,
        }
    };
}
