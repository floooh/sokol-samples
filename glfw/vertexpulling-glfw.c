//------------------------------------------------------------------------------
//  vertexpulling-glfw.c
//
//  Demonstrates vertex pulling via storage buffers.
//  Requires GL 4.3.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

typedef struct {
    float pos[4];
    float color[4];
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

int main() {
    glfw_init(&(glfw_desc_t){
        .title = "vertexpulling-glfw.c",
        .width = 640,
        .height = 480,
        .sample_count = 4,
        .version_major = 4,
        .version_minor = 3,
    });
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });

    const sg_pass_action pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f } },
    };

    // create a stroage buffer with the vertex data
    const vertex_t vertices[] = {
        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },

        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },

        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },

        { .pos = { 1.0, -1.0, -1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0,  1.0, -1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0,  1.0,  1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0, -1.0,  1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },

        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },

        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } }
    };
    sg_buffer sbuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_STORAGEBUFFER,
        .data = SG_RANGE(vertices),
    });

    // ... and an index buffer
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    // a shader where the vertex shader pulls the vertex data from an SSBO
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name = "mvp", .type = SG_UNIFORMTYPE_MAT4 },
            },
        },
        .vs.storage_buffers[0] = { .used = true, .readonly = true },
        .vs.source =
            "#version 430\n"
            "uniform mat4 mvp;\n"
            "struct vertex_t {\n"
            "  vec4 pos;\n"
            "  vec4 color;\n"
            "};\n"
            "layout(std430, binding=0) readonly buffer ssbo {\n"
            "  vertex_t vtx[];\n"
            "};\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  gl_Position = mvp * vtx[gl_VertexID].pos;\n"
            "  color = vtx[gl_VertexID].color;\n"
            "}\n",
        .fs.source =
            "#version 430\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n"
    });

    // a pipeline object, note that there is no vertex layout specification
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    // resource bindings, note the lack of a vertex buffer and instead
    // the storage buffer is bound to the vertex stage
    sg_bindings bind = {
        .index_buffer = ibuf,
        .vs.storage_buffers[0] = sbuf,
    };

    vs_params_t vs_params;
    float rx = 0.0f, ry = 0.0f;
    while (!glfwWindowShouldClose(glfw_window())) {
        // view-projection matrix
        hmm_mat4 proj = HMM_Perspective(60.0f, (float)glfw_width()/(float)glfw_height(), 0.01f, 10.0f);
        hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

        // rotated model matrix
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);

        // model-view-projection matrix for vertex shader
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    sg_shutdown();
    glfwTerminate();
}
