//------------------------------------------------------------------------------
//  cube-glfw.c
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_USE_GL
#include "sokol_gfx.h"

/* a uniform block with a model-view-projection matrix */
typedef struct {
    hmm_mat4 mvp;
} params_t;

int main() {
    const int WIDTH = 640;
    const int HEIGHT = 480;

    /* create GLFW window and initialize GL */
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
    sg_setup(&desc);
    assert(sg_isvalid());

    /* cube vertex buffer */
    float vertices[] = {
        -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0, 
         1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
        -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

        -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0, 
         1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0, 
        -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0, 
        -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0, 
        -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

        1.0, -1.0, -1.0,   1.0, 1.0, 0.0, 1.0, 
        1.0,  1.0, -1.0,   1.0, 1.0, 0.0, 1.0, 
        1.0,  1.0,  1.0,   1.0, 1.0, 0.0, 1.0, 
        1.0, -1.0,  1.0,   1.0, 1.0, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 1.0, 1.0, 1.0, 
        -1.0, -1.0,  1.0,   0.0, 1.0, 1.0, 1.0, 
         1.0, -1.0,  1.0,   0.0, 1.0, 1.0, 1.0, 
         1.0, -1.0, -1.0,   0.0, 1.0, 1.0, 1.0,

        -1.0,  1.0, -1.0,   1.0, 0.0, 1.0, 1.0, 
        -1.0,  1.0,  1.0,   1.0, 0.0, 1.0, 1.0, 
         1.0,  1.0,  1.0,   1.0, 0.0, 1.0, 1.0, 
         1.0,  1.0, -1.0,   1.0, 0.0, 1.0, 1.0
    };
    sg_buffer_desc vbuf_desc;
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = sizeof(vertices);
    vbuf_desc.data_ptr = vertices;
    vbuf_desc.data_size = sizeof(vertices);
    sg_id vbuf = sg_make_buffer(&vbuf_desc);

    /* create an index buffer for the cube */
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        4, 5, 6,  4, 6, 7,
        8, 9, 10,  8, 10, 11,
        12, 13, 14,  12, 14, 15,
        16, 17, 18,  16, 18, 19,
        20, 21, 22,  20, 22, 23
    };
    sg_buffer_desc ibuf_desc;
    sg_init_buffer_desc(&ibuf_desc);
    ibuf_desc.size = sizeof(indices);
    ibuf_desc.data_ptr = indices;
    ibuf_desc.data_size = sizeof(indices);
    sg_id ibuf = sg_make_buffer(&ibuf_desc);

    /* create shader */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc.vs.ub[0], sizeof(params_t));
    sg_init_uniform(&shd_desc.vs.ub[0].u[0], "mvp", offsetof(params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    shd_desc.vs.source = 
        "#version 330\n"
        "uniform mat4 mvp;\n"
        "in vec4 position;\n"
        "in vec4 color0;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  gl_Position = mvp * position;\n"
        "  color = color0;\n"
        "}\n";
    shd_desc.fs.source =
        "#version 330\n"
        "in vec4 color;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "  frag_color = color;\n"
        "}\n";
    sg_id shd = sg_make_shader(&shd_desc);

    /* create pipeline object */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_pipeline_desc_named_attr(&pip_desc, 0, "position", SG_VERTEXFORMAT_FLOAT3);
    sg_pipeline_desc_named_attr(&pip_desc, 0, "color0", SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    sg_id pip = sg_make_pipeline(&pip_desc);

    /* draw state struct with resource bindings */
    sg_draw_state draw_state;
    sg_init_draw_state(&draw_state);
    draw_state.pipeline = pip;
    draw_state.vertex_buffers[0] = vbuf;
    draw_state.index_buffer = ibuf;

    /* default pass action */
    sg_pass_action pass_action;
    sg_init_pass_action(&pass_action);

    params_t params;
    while (!glfwWindowShouldClose(w)) {
        sg_begin_pass(SG_DEFAULT_PASS, &pass_action, WIDTH, HEIGHT);
        sg_apply_draw_state(&draw_state);
        sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &params, sizeof(params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    sg_destroy_pipeline(pip);
    sg_destroy_shader(shd);
    sg_destroy_buffer(ibuf);
    sg_destroy_buffer(vbuf);
    sg_shutdown();
    glfwTerminate();
}
