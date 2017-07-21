//------------------------------------------------------------------------------
//  clear-glfw.c
//------------------------------------------------------------------------------
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_USE_GL
#include "sokol_gfx.h"

int main() {

    const int WIDTH = 640;
    const int HEIGHT = 480;

    /* create window and GL context via GLFW */
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Clear GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit(w);

    /* setup sokol_gfx */
    sg_desc desc;
    sg_init_desc(&desc);
    sg_setup(&desc);
    assert(sg_isvalid());

    /* default pass action, clear to red */
    sg_pass_action pass_action;
    sg_init_pass_action(&pass_action);
    pass_action.color[0][0] = 1.0f;
    pass_action.color[0][1] = 0.0f;
    pass_action.color[0][2] = 0.0f;
    pass_action.actions = SG_PASSACTION_CLEAR_ALL;

    /* draw loop */
    while (!glfwWindowShouldClose(w)) {
        float g = pass_action.color[0][1] + 0.01;
        if (g > 1.0f) g = 0.0f;
        pass_action.color[0][1] = g;
        sg_begin_pass(SG_DEFAULT_PASS, &pass_action, WIDTH, HEIGHT);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    /* shutdown sokol_gfx and GLFW */
    sg_shutdown();
    glfwTerminate();
    return 0;
}
