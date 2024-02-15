#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__cplusplus)
extern "C" {
#endif
void glfw_init(const char* title, int w, int h, int sample_count);
GLFWwindow* glfw_window(void);
int glfw_width(void);
int glfw_height(void);
sg_environment glfw_environment(void);
sg_swapchain glfw_swapchain(void);
#if defined(__cplusplus)
} // extern "C"
#endif
