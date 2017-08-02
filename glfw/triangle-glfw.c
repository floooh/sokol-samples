//------------------------------------------------------------------------------
//  triangle-glfw.c
//  Vertex buffer, simple shader, pipeline state object.
//------------------------------------------------------------------------------
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
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
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Triangle GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit(w);

    /* setup sokol_gfx */
    sg_desc desc = {0}; 
    sg_setup(&desc);
    assert(sg_isvalid());

    /* create a vertex buffer with 3 vertices */
    float vertices[] = {
        // positions            // colors
         0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f 
    };
    sg_buffer_desc buf_desc = {
        .size = sizeof(vertices),
        .data_ptr = vertices,   
        .data_size = sizeof(vertices)
    };
    sg_buffer buf_id = sg_make_buffer(&buf_desc);

    /* create a shader */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    shd_desc.vs.source = 
        "#version 330\n"
        "in vec4 position;\n"
        "in vec4 color0;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  gl_Position = position;\n"
        "  color = color0;\n"
        "}\n";
    shd_desc.fs.source = 
        "#version 330\n"
        "in vec4 color;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "  frag_color = color;\n"
        "}\n";
    sg_shader shd_id = sg_make_shader(&shd_desc);

    /* create a pipeline object (default render states are fine for triangle) */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 28);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = shd_id;
    sg_pipeline pip_id = sg_make_pipeline(&pip_desc);

    /* draw state struct defines the resource bindings */
    sg_draw_state draw_state = {
        .pipeline = pip_id,
        .vertex_buffers[0] = buf_id
    };

    /* default pass action (clear to grey) */
    sg_pass_action pass_action;
    sg_init_pass_action(&pass_action);

    /* draw loop */
    while (!glfwWindowShouldClose(w)) {
        sg_begin_default_pass(&pass_action, WIDTH, HEIGHT);
        sg_apply_draw_state(&draw_state);
        sg_draw(0, 3, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    /* cleanup */
    sg_destroy_pipeline(pip_id);
    sg_destroy_shader(shd_id);
    sg_destroy_buffer(buf_id);
    sg_shutdown();
    glfwTerminate();
    return 0;
}
