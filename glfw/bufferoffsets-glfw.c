//------------------------------------------------------------------------------
//  bufferoffsets-glfw.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"

typedef struct {
    float x, y, r, g, b;
} vertex_t;

int main() {
    const int WIDTH = 640;
    const int HEIGHT = 480;

    /* create GLFW window and initialize GL */
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Buffer Offsets GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit();

    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());

    /* a 2D triangle and quad in 1 vertex buffer and 1 index buffer */
    vertex_t vertices[7] = {
        /* triangle */
        {  0.0f,   0.55f,  1.0f, 0.0f, 0.0f },
        {  0.25f,  0.05f,  0.0f, 1.0f, 0.0f },
        { -0.25f,  0.05f,  0.0f, 0.0f, 1.0f },

        /* quad */
        { -0.25f, -0.05f,  0.0f, 0.0f, 1.0f },
        {  0.25f, -0.05f,  0.0f, 1.0f, 0.0f },
        {  0.25f, -0.55f,  1.0f, 0.0f, 0.0f },
        { -0.25f, -0.55f,  1.0f, 1.0f, 0.0f }        
    };
    uint16_t indices[9] = {
        0, 1, 2,
        0, 1, 2, 0, 2, 3
    };
    sg_buffer vb = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });
    sg_buffer ib = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* create a shader to render 2D colored shapes */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "#version 330\n"
            "in vec2 pos;"
            "in vec3 color0;"
            "out vec4 color;"
            "void main() {"
            "  gl_Position = vec4(pos, 0.5, 1.0);\n"
            "  color = vec4(color0, 1.0);\n"
            "}\n",
        .fs.source =
            "#version 330\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n"
    });

    /* a pipeline state object, default states are fine */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0] = { .name="pos", .format=SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .name="color0", .format=SG_VERTEXFORMAT_FLOAT3 }
            }
        }
    });

    /* a draw state object with the resource bindings, before drawing,
       the buffer offsets will be updates
    */
   sg_draw_state ds = {
       .pipeline = pip,
       .vertex_buffers[0] = vb,
       .index_buffer = ib
   };

    /* a pass action to clear to blue-ish */
    sg_pass_action pass_action = {
        .colors = {
            [0] = { .action=SG_ACTION_CLEAR, .val = { 0.5f, 0.5f, 1.0f, 1.0f } }
        }
    };

    while (!glfwWindowShouldClose(w)) {
        int cur_width, cur_height;
        glfwGetFramebufferSize(w, &cur_width, &cur_height);
        sg_begin_default_pass(&pass_action, cur_width, cur_height);
        /* render the triangle */
        ds.vertex_buffer_offsets[0] = 0;
        ds.index_buffer_offset = 0;
        sg_apply_draw_state(&ds);
        sg_draw(0, 3, 1);
        /* render the quad from the same vertex- and index-buffer */
        ds.vertex_buffer_offsets[0] = 3 * sizeof(vertex_t);
        ds.index_buffer_offset = 3 * sizeof(uint16_t);
        sg_apply_draw_state(&ds);
        sg_draw(0, 6, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }
    sg_shutdown();
    glfwTerminate();
}

