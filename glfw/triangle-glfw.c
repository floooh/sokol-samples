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

    /* default pass action (clears to grey) */
    sg_pass_action pass_action = {0};

    /* vertex data for the triangle */
    const float vertices[] = {
        // positions            // colors
         0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f 
    };

    /*
        Ok, this is maybe a bit ridiculous.

        The following initializes a sg_draw_state struct containing
        a pipeline and buffer resource, and creates these resources
        'in-place'.
    */
    sg_draw_state draw_state = {
        .pipeline = sg_make_pipeline(&(sg_pipeline_desc){
            .vertex_layouts[0] = {
                .stride = 28,
                .attrs = {
                    [0] = { .name="position", .offset=0, .format=SG_VERTEXFORMAT_FLOAT3 },
                    [1] = { .name="color0", .offset=12, .format=SG_VERTEXFORMAT_FLOAT4 }
                }
            },
            .shader = sg_make_shader(&(sg_shader_desc){
                .vs.source = 
                    "#version 330\n"
                    "in vec4 position;\n"
                    "in vec4 color0;\n"
                    "out vec4 color;\n"
                    "void main() {\n"
                    "  gl_Position = position;\n"
                    "  color = color0;\n"
                    "}\n",
                .fs.source =
                    "#version 330\n"
                    "in vec4 color;\n"
                    "out vec4 frag_color;\n"
                    "void main() {\n"
                    "  frag_color = color;\n"
                    "}\n"
            })
        }),
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .size = sizeof(vertices),
            .data_ptr = vertices, 
        })
    };

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
    sg_shutdown();
    glfwTerminate();
    return 0;
}
