#include "sokol_gfx.h"
#include "glfw_glue.h"
#include "assert.h"

static int _sample_count;
static bool _no_depth_buffer;
static int _major_version;
static int _minor_version;
static GLFWwindow* _window;


#define _glfw_def(val, def) (((val) == 0) ? (def) : (val))

void glfw_init(const glfw_desc_t* desc) {
    assert(desc);
    assert(desc->width > 0);
    assert(desc->height > 0);
    assert(desc->title);
    glfw_desc_t desc_def = *desc;
    desc_def.sample_count = _glfw_def(desc_def.sample_count, 1);
    desc_def.version_major = _glfw_def(desc_def.version_major, 4);
    desc_def.version_minor = _glfw_def(desc_def.version_minor, 1);
    _sample_count = desc_def.sample_count;
    _no_depth_buffer = desc_def.no_depth_buffer;
    _major_version = desc_def.version_major;
    _minor_version = desc_def.version_minor;
    glfwInit();
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, 0);
    if (desc_def.no_depth_buffer) {
        glfwWindowHint(GLFW_DEPTH_BITS, 0);
        glfwWindowHint(GLFW_STENCIL_BITS, 0);
    }
    glfwWindowHint(GLFW_SAMPLES, (desc_def.sample_count == 1) ? 0 : desc_def.sample_count);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, desc_def.version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, desc_def.version_minor);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    _window = glfwCreateWindow(desc_def.width, desc_def.height, desc_def.title, 0, 0);
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
            .depth_format = _no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
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
        .depth_format = _no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
        .gl = {
            // we just assume here that the GL framebuffer is always 0
            .framebuffer = 0,
        }
    };
}
