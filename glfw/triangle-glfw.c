//------------------------------------------------------------------------------
//  triangle-glfw.c
//------------------------------------------------------------------------------
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_USE_GL
#include "sokol_gfx.h"
#include <stdio.h>

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
    desc.width = WIDTH;
    desc.height = HEIGHT;
    sg_setup(&desc);
    assert(sg_isvalid());

    /* use default pass action (clear to grey) */
    sg_pass_action pass_action;
    sg_init_pass_action(&pass_action);

    /* create a vertex buffer with 3 vertices */
    float vertices[] = {
        // positions            // colors
         0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f 
    };
    sg_buffer_desc buf_desc;
    sg_init_buffer_desc(&buf_desc);
    buf_desc.size = sizeof(vertices);
    buf_desc.data_ptr = vertices;
    buf_desc.data_size = sizeof(vertices);
    sg_id buf_id = sg_make_buffer(&buf_desc);

    /* create a shader */

    /* create a pipeline object */

    /* draw loop */
    while (!glfwWindowShouldClose(w)) {
        sg_begin_pass(SG_DEFAULT_PASS, &pass_action, WIDTH, HEIGHT);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    /* cleanup */
    sg_destroy_buffer(buf_id);
    sg_discard();
    glfwTerminate();
}
