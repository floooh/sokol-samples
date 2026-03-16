//------------------------------------------------------------------------------
//  vertexpulling-glfw.c
//
//  Demonstrates vertex pulling via storage buffers.
//  Requires GL 4.3.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

typedef struct {
    float pos[4];
    float color[4];
} vertex_t;

typedef struct {
    mat44_t mvp;
} vs_params_t;

static mat44_t compute_mvp(float rx, float ry, int width, int height) {
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)width/(float)height, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, view_proj);
}

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

    // create a buffer with the vertex data and a storage buffer view
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
        .usage.storage_buffer = true,
        .data = SG_RANGE(vertices),
    });
    sg_view sbuf_view = sg_make_view(&(sg_view_desc){ .storage_buffer.buffer = sbuf });

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
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // a shader where the vertex shader pulls the vertex data from an SSBO
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
        .fragment_func.source =
            "#version 430\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = {.glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 },
            },
        },
        .views[0].storage_buffer = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .readonly = true,
            .glsl_binding_n = 0,
        },
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
        .views[0] = sbuf_view,
    };

    float rx = 0.0f, ry = 0.0f;
    while (!glfwWindowShouldClose(glfw_window())) {
        rx += 1.0f; ry += 2.0f;
        const vs_params_t vs_params = { .mvp = compute_mvp(rx, ry, glfw_width(), glfw_height()) };
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    sg_shutdown();
    glfwTerminate();
}
