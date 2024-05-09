#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct glfw_desc_t {
    int width;
    int height;
    int sample_count;
    bool no_depth_buffer;
    const char* title;
    int version_major;
    int version_minor;
} glfw_desc_t;

void glfw_init(const glfw_desc_t* desc);
GLFWwindow* glfw_window(void);
int glfw_width(void);
int glfw_height(void);
sg_environment glfw_environment(void);
sg_swapchain glfw_swapchain(void);

#if defined(__cplusplus)
} // extern "C"
#endif
