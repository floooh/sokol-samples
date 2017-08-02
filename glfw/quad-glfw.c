//------------------------------------------------------------------------------
//  quad-glfw.c
//  Indexed drawing, explicit vertex attr locations.
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
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Quad GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit(w);

    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());

    /* create a vertex buffer */
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,        
    };
    sg_buffer_desc vbuf_desc = {
        .size = sizeof(vertices),
        .data_ptr = vertices,
        .data_size = sizeof(vertices)
    };
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);

    /* create an index buffer */
    uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle        
    };
    sg_buffer_desc ibuf_desc = {
        .size = sizeof(indices),
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data_ptr = indices,
        .data_size = sizeof(indices)
    };
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

    /* create a shader (use vertex attribute locations) */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    shd_desc.vs.source = 
        "#version 330\n"
        "layout(location=0) in vec4 position;\n"
        "layout(location=1) in vec4 color0;\n"
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
    sg_shader shd = sg_make_shader(&shd_desc);
    
    /* create a pipeline object (default render state is fine) */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    /* vertex attrs can also be bound by location instead of name (but not in GLES2) */
    sg_init_vertex_stride(&pip_desc, 0, 28);
    sg_init_indexed_vertex_attr(&pip_desc, 0, 0, 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_indexed_vertex_attr(&pip_desc, 0, 1, 12, SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    sg_pipeline pip = sg_make_pipeline(&pip_desc);

    /* draw state struct with resource bindings */
    sg_draw_state draw_state = {
        .pipeline = pip,
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* default pass action */
    sg_pass_action pass_action;
    sg_init_pass_action(&pass_action);

    /* draw loop */
    while (!glfwWindowShouldClose(w)) {
        sg_begin_default_pass(&pass_action, WIDTH, HEIGHT);
        sg_apply_draw_state(&draw_state);
        sg_draw(0, 6, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    /* cleanup */
    sg_destroy_pipeline(pip);
    sg_destroy_shader(shd);
    sg_destroy_buffer(ibuf);
    sg_destroy_buffer(vbuf);
    sg_shutdown();
    glfwTerminate();

    return 0;
}