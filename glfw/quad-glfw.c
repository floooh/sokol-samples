//------------------------------------------------------------------------------
//  quad-glfw.c
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
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Quad GLFW", 0, 0);
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

    /* create a vertex buffer */
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,        
    };
    sg_buffer_desc vbuf_desc;
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = sizeof(vertices);
    vbuf_desc.data_ptr = vertices;
    vbuf_desc.data_size = sizeof(vertices);
    sg_id vbuf_id = sg_make_buffer(&vbuf_desc);

    /* create an index buffer */
    uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle        
    };
    sg_buffer_desc ibuf_desc;
    sg_init_buffer_desc(&ibuf_desc);
    ibuf_desc.size = sizeof(indices);
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.data_ptr = indices;
    ibuf_desc.data_size = sizeof(indices);
    sg_id ibuf_id = sg_make_buffer(&ibuf_desc);

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
    sg_id shd_id = sg_make_shader(&shd_desc);
    assert(shd_id);
    
    /* create a pipeline object (default render state is fine) */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_pipeline_desc_named_attr(&pip_desc, 0, "position", SG_VERTEXFORMAT_FLOAT3);
    sg_pipeline_desc_named_attr(&pip_desc, 0, "color0", SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = shd_id;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    sg_id pip_id = sg_make_pipeline(&pip_desc);
    assert(pip_id);

    /* draw state struct with resource bindings */
    sg_draw_state draw_state;
    sg_init_draw_state(&draw_state);
    draw_state.pipeline = pip_id;
    draw_state.vertex_buffers[0] = vbuf_id;
    draw_state.index_buffer = ibuf_id;

    /* default pass action */
    sg_pass_action pass_action;
    sg_init_pass_action(&pass_action);

    /* draw loop */
    while (!glfwWindowShouldClose(w)) {
        sg_begin_pass(SG_DEFAULT_PASS, &pass_action, WIDTH, HEIGHT);
        sg_apply_draw_state(&draw_state);
        sg_draw(0, 6, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    /* cleanup */
    sg_destroy_pipeline(pip_id);
    sg_destroy_shader(shd_id);
    sg_destroy_buffer(ibuf_id);
    sg_destroy_buffer(vbuf_id);
    sg_discard();
    glfwTerminate();

    return 0;
}