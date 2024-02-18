//------------------------------------------------------------------------------
//  clear-glfw.c
//  Simple draw loop, clear default framebuffer.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

int main() {

    // setup GLFW and sokol-gfx
    glfw_init(&(glfw_desc_t){ .title = "clear-glfw.c", .width = 640, .height = 460 });
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });
    assert(sg_isvalid());

    // default pass action, clear to red
    sg_pass_action pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    // draw loop
    while (!glfwWindowShouldClose(glfw_window())) {
        float g = (float)(pass_action.colors[0].clear_value.g + 0.01);
        if (g > 1.0f) g = 0.0f;
        pass_action.colors[0].clear_value.g = g;
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    // shutdown sokol_gfx and GLFW
    sg_shutdown();
    glfwTerminate();
    return 0;
}
