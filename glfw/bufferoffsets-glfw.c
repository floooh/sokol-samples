//------------------------------------------------------------------------------
//  bufferoffsets-glfw.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

typedef struct {
    float x, y, r, g, b;
} vertex_t;

int main() {
    // create GLFW window and initialize GL
    glfw_init("bufferoffsets-glfw.c", 640, 480, 1);

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });
    assert(sg_isvalid());

    // a 2D triangle and quad in 1 vertex buffer and 1 index buffer
    vertex_t vertices[7] = {
        // triangle
        {  0.0f,   0.55f,  1.0f, 0.0f, 0.0f },
        {  0.25f,  0.05f,  0.0f, 1.0f, 0.0f },
        { -0.25f,  0.05f,  0.0f, 0.0f, 1.0f },

        // quad
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
        .data = SG_RANGE(vertices)
    });
    sg_buffer ib = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    // setup resource bindings struct */
    sg_bindings bind = {
        .vertex_buffers[0] = vb,
        .index_buffer = ib
    };

    // create a shader to render 2D colored shapes
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "#version 330\n"
            "layout(location=0) in vec2 pos;"
            "layout(location=1) in vec3 color0;"
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

    // a pipeline state object, default states are fine
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT2,
                [1].format=SG_VERTEXFORMAT_FLOAT3
            }
        }
    });

    // a pass action to clear to blue-ish
    sg_pass_action pass_action = {
        .colors = {
            [0] = { .load_action=SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f } }
        }
    };

    while (!glfwWindowShouldClose(glfw_window())) {
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        // render the triangle
        bind.vertex_buffer_offsets[0] = 0;
        bind.index_buffer_offset = 0;
        sg_apply_bindings(&bind);
        sg_draw(0, 3, 1);
        // render the quad from the same vertex- and index-buffer
        bind.vertex_buffer_offsets[0] = 3 * sizeof(vertex_t);
        bind.index_buffer_offset = 3 * sizeof(uint16_t);
        sg_apply_bindings(&bind);
        sg_draw(0, 6, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }
    sg_shutdown();
    glfwTerminate();
}
