//------------------------------------------------------------------------------
//  instancing-compute-glfw.c
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_log.h"
#include "glfw_glue.h"

#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

typedef struct {
    uint64_t last_time;
    int num_particles;
    float ry;
    struct {
        sg_buffer buf;
        sg_pipeline pip;
    } compute;
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
} state_t;

typedef struct {
    float dt;
    int32_t num_particles;
} cs_params_t;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

typedef struct {
    float pos[4];
    float vel[4];
} particle_t;

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

int main() {

    state_t state = {
        .display.pass_action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1} },
        },
    };
    glfw_init(&(glfw_desc_t){
        .title = "instancing-compute-glfw.c",
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .version_major = 4,
        .version_minor = 3,
    });
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });
    stm_setup();

    // a storage buffer which holds the particle positions and velocities,
    // updated by a compute shader
    {
        particle_t* p = calloc(MAX_PARTICLES, sizeof(particle_t));
        assert(p);
        for (size_t i = 0; i < MAX_PARTICLES; i++) {
            p[i].vel[0] = ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f;
            p[i].vel[1] = ((float)(xorshift32() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f;
            p[i].vel[2] = ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f;
        }
        state.compute.buf = sg_make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_STORAGEBUFFER,
            .data = {
                .ptr = p,
                .size = MAX_PARTICLES * sizeof(particle_t),
            },
            .label = "particle-buffer",
        });
        free(p);
    }

    // create compute shader to update particle position and velocity
    sg_shader compute_shd = sg_make_shader(&(sg_shader_desc){
        .compute_func.source =
            "#version 430\n"
            "uniform float dt;\n"
            "uniform int num_particles;\n"
            "struct particle_t {\n"
            "  vec4 pos;\n"
            "  vec4 vel;\n"
            "};\n"
            "layout(std430, binding=0) buffer ssbo {\n"
            "  particle_t prt[];\n"
            "};\n"
            "layout(local_size_x=64, local_size_y=1, local_size_y=1) in;"
            "void main() {\n"
            "  uint idx = gl_GlobalInvocationID.x;\n"
            "  if (idx >= num_particles) {\n"
            "    return;\n"
            "  }\n"
            "  vec4 pos = prt[idx].pos;\n"
            "  vec4 vel = prt[idx].vel;\n"
            "  vel.y -= 1.0 * dt;\n"
            "  pos += vel * dt;\n"
            "  if (pos.y < -2.0) {\n"
            "    pos.y = -1.8;\n"
            "    vel *= vec4(0.8, -0.8, 0.8, 0);\n"
            "  }\n"
            "  prt[idx].pos = pos;\n"
            "  prt[idx].vel = vel;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_COMPUTE,
            .size = sizeof(cs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "dt", .type = SG_UNIFORMTYPE_FLOAT },
                [1] = { .glsl_name = "num_particles", .type = SG_UNIFORMTYPE_INT },
            },
        },
        .storage_buffers[0] = {
            .stage = SG_SHADERSTAGE_COMPUTE,
            .readonly = false,
            .glsl_binding_n = 0,
        },
        .label = "compute-shader",
    });

    // create a compute-pipeline object
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = compute_shd,
        .label = "compute-pipeline",
    });

    // vertex buffer for static per-particle geometry
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
    state.display.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "geometry-vbuf",
    });

    // index buffer for static per-particle geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.display.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "geometry-ibuf",
    });

    // shader for rendering with instance-positions pulled from the storage buffer
    sg_shader display_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 430\n"
            "uniform mat4 mvp;\n"
            "struct particle_t {\n"
            "  vec4 pos;\n"
            "  vec4 vel;\n"
            "};\n"
            "layout(std430, binding=0) readonly buffer ssbo {\n"
            "  particle_t prt[];\n"
            "};\n"
            "layout(location=0) in vec3 position;\n"
            "layout(location=1) in vec4 color0;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  vec3 inst_pos = prt[gl_InstanceID].pos.xyz;\n"
            "  vec4 pos = vec4(position + inst_pos, 1.0);\n"
            "  gl_Position = mvp * pos;\n"
            "  color = color0;\n"
            "}\n",
        .fragment_func.source =
            "#version 430\n"
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
            },
        },
        .storage_buffers[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .readonly = true,
            .glsl_binding_n = 0,
        },
        .label = "render-shader",
    });

    // a pipeline object for rendering, this uses geometry from
    // traditional vertex buffer, but pull per-instance positions
    // from the compute-update storage buffer
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 }, // position
                [1] = { .format = SG_VERTEXFORMAT_FLOAT4 }, // color
            },
        },
        .shader = display_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
        .label = "render-pipeline",
    });

    // draw loop
    while (!glfwWindowShouldClose(glfw_window())) {
        state.num_particles += NUM_PARTICLES_EMITTED_PER_FRAME;
        if (state.num_particles > MAX_PARTICLES) {
            state.num_particles = MAX_PARTICLES;
        }

        // compute pass to update particle positions
        const cs_params_t cs_params = {
            .dt = (float)stm_sec(stm_laptime(&state.last_time)),
            .num_particles = state.num_particles,
        };
        sg_begin_pass(&(sg_pass){ .compute = true, .label = "compute-pass" });
        sg_apply_pipeline(state.compute.pip);
        sg_apply_bindings(&(sg_bindings){
            .storage_buffers[0] = state.compute.buf,
        });
        sg_apply_uniforms(0, &SG_RANGE(cs_params));
        sg_dispatch((state.num_particles + 63)/64, 1, 1);
        sg_end_pass();

        // render pass to render the instanced particles, with the
        // instance-positions pulled from the compute-updated storage buffer
        state.ry += 1;
        const hmm_mat4 proj = HMM_Perspective(60.0f, (float)glfw_width()/(float)glfw_height(), 0.01f, 50.0f);
        const hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        const hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
        const vs_params_t vs_params = {
            .mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f)))
        };
        sg_begin_pass(&(sg_pass){
            .action = state.display.pass_action,
            .swapchain = glfw_swapchain(),
            .label = "render-pass",
        });
        sg_apply_pipeline(state.display.pip);
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.display.vbuf,
            .index_buffer = state.display.ibuf,
            .storage_buffers[0] = state.compute.buf,
        });
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 24, state.num_particles);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }
    sg_shutdown();
    glfwTerminate();
}
