//------------------------------------------------------------------------------
//  triangle-glfw.c
//  Vertex buffer, simple shader, pipeline state object.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

int main() {

    // create window and GL context via GLFW
    glfw_init(&(glfw_desc_t){ .title = "triangle-glfw.c", .width = 640, .height = 480, .no_depth_buffer = true });
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });

    // a vertex buffer
    const float vertices[] = {
        // positions            // colors
         0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // a shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "#version 330\n"
            "layout(location=0) in vec4 position;\n"
            "layout(location=1) in vec4 color0;\n"
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
    });

    // a pipeline state object (default render states are fine for triangle)
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            }
        }
    });

    // resource bindings
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf
    };

    // draw loop
    while (!glfwWindowShouldClose(glfw_window())) {
        sg_begin_pass(&(sg_pass){ .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_draw(0, 3, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    // cleanup
    sg_shutdown();
    glfwTerminate();
    return 0;
}
