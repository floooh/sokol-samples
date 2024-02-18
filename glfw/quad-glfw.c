//------------------------------------------------------------------------------
//  quad-glfw.c
//  Indexed drawing, explicit vertex attr locations.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

int main() {

    // create window and GL context via GLFW
    glfw_init(&(glfw_desc_t){ .title = "quad-glfw.c", .width = 640, .height = 480 });

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });
    assert(sg_isvalid());

    // create a vertex buffer
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
    };
    sg_buffer_desc vbuf_desc = {
        .data = SG_RANGE(vertices)
    };
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);

    // create an index buffer
    uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle
    };
    sg_buffer_desc ibuf_desc = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    };
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

    // define the resource bindings
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    // create a shader (use vertex attribute locations)
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

    // create a pipeline object (default render state is fine)
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            // test to provide attr offsets, but no buffer stride, this should compute the stride
            .attrs = {
                // vertex attrs can also be bound by location instead of name
                [0] = { .offset = 0, .format = SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset = 12, .format = SG_VERTEXFORMAT_FLOAT4 }
            }
        }
    });

    // draw loop
    while (!glfwWindowShouldClose(glfw_window())) {
        sg_begin_pass(&(sg_pass){ .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_draw(0, 6, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    /* cleanup */
    sg_shutdown();
    glfwTerminate();

    return 0;
}
