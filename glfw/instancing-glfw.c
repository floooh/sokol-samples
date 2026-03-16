//------------------------------------------------------------------------------
//  instancing-glfw.c
//  Instanced rendering, static geometry vertex- and index-buffers,
//  dynamically updated instance-data buffer.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

enum {
    MAX_PARTICLES = 512*1024,
    NUM_PARTICLES_EMITTED_PER_FRAME = 10
};

// vertex shader uniform block
typedef struct {
    mat44_t mvp;
} vs_params_t;

// particle positions and velocity
static int cur_num_particles = 0;
static vec3_t pos[MAX_PARTICLES];
static vec3_t vel[MAX_PARTICLES];

int main() {

    // create window and GL context via GLFW
    glfw_init(&(glfw_desc_t){ .title = "instancing-glfw.c", .width = 800, .height = 600, .sample_count = 4 });
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });
    assert(sg_isvalid());

    // vertex buffer for static geometry (goes into vertex buffer bind slot 0)
    const float r = 0.05f;
    const float vertices[] = {
        // positions            colors
        0.0f,   -r, 0.0f,       1.0f, 0.0f, 0.0f, 1.0f,
           r, 0.0f, r,          0.0f, 1.0f, 0.0f, 1.0f,
           r, 0.0f, -r,         0.0f, 0.0f, 1.0f, 1.0f,
          -r, 0.0f, -r,         1.0f, 1.0f, 0.0f, 1.0f,
          -r, 0.0f, r,          0.0f, 1.0f, 1.0f, 1.0f,
        0.0f,    r, 0.0f,       1.0f, 0.0f, 1.0f, 1.0f
    };
    sg_buffer vbuf_geom = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // index buffer for static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // empty, dynamic instance-data vertex buffer (goes into vertex buffer bind slot 1)
    sg_buffer vbuf_inst = sg_make_buffer(&(sg_buffer_desc){
        .usage.stream_update = true,
        .size = MAX_PARTICLES * sizeof(vec3_t),
    });

    // create a shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 410\n"
            "uniform mat4 mvp;\n"
            "layout(location=0) in vec3 position;\n"
            "layout(location=1) in vec4 color0;\n"
            "layout(location=2) in vec3 instance_pos;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  vec4 pos = vec4(position + instance_pos, 1.0);"
            "  gl_Position = mvp * pos;\n"
            "  color = color0;\n"
            "}\n",
        .fragment_func.source =
            "#version 410\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 },
            }
        },
    });

    // pipeline state object, note the vertex layout definition
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers = {
                [0] = { .stride = 28 },
                [1] = { .stride = 12, .step_func=SG_VERTEXSTEP_PER_INSTANCE }
            },
            .attrs = {
                [0] = { .offset = 0,  .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },
                [1] = { .offset = 12, .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },
                [2] = { .offset = 0,  .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK
    });

    // setup resource bindings, note how the instance-data buffer goes into vertex buffer slot 1
    sg_bindings bind = {
        .vertex_buffers = {
            [0] = vbuf_geom,
            [1] = vbuf_inst
        },
        .index_buffer = ibuf
    };

    // draw loop
    float ry = 0.0f;
    const float frame_time = 1.0f / 60.0f;
    while (!glfwWindowShouldClose(glfw_window())) {
        // emit new particles
        for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
            if (cur_num_particles < MAX_PARTICLES) {
                pos[cur_num_particles] = vec3(0.0, 0.0, 0.0);
                vel[cur_num_particles] = vec3(
                    ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f,
                    ((float)(rand() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f,
                    ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f);
                cur_num_particles++;
            }
            else {
                break;
            }
        }

        // update particle positions
        for (int i = 0; i < cur_num_particles; i++) {
            vel[i].y -= 1.0f * frame_time;
            pos[i].x += vel[i].x * frame_time;
            pos[i].y += vel[i].y * frame_time;
            pos[i].z += vel[i].z * frame_time;
            if (pos[i].y < -2.0f) {
                pos[i].y = -1.8f;
                vel[i].y = -vel[i].y;
                vel[i].x *= 0.8f; vel[i].y *= 0.8f; vel[i].z *= 0.8f;
            }
        }

        // update instance data
        sg_update_buffer(bind.vertex_buffers[1], &(sg_range) {
            .ptr = pos,
            .size = (size_t)cur_num_particles * sizeof(vec3_t)
        });

        // model-view-projection matrix
        ry += 1.0f;
        const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)glfw_width()/(float)glfw_height(), 0.01f, 50.0f);
        const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 8.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat44_t model = mat44_rotation_y(vm_radians(ry));
        const vs_params_t vs_params = { .mvp = vm_mul(model, vm_mul(view, proj)) };

        sg_begin_pass(&(sg_pass){ .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 24, cur_num_particles);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    // cleanup
    sg_shutdown();
    glfwTerminate();
}
