//------------------------------------------------------------------------------
//  instancing-emsc.c
//------------------------------------------------------------------------------
#include <stddef.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"

#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    float roty;
    int cur_num_particles;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
} state = {
    // clear to black
    .pass_action.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
};

// particle positions and velocity
static hmm_vec3 pos[MAX_PARTICLES];
static hmm_vec3 vel[MAX_PARTICLES];

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static EM_BOOL draw(double time, void* userdata);

int main() {
    // setup WebGL context
    emsc_init("#canvas", EMSC_ANTIALIAS);

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = emsc_environment(),
        .logger.func = slog_func
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
    sg_buffer geom_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // index buffer for static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    sg_buffer ibuf= sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // empty, dynamic instance-data vertex buffer (goes into vertex buffer bind slot 1)
    sg_buffer inst_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.stream_update = true,
        .size = MAX_PARTICLES * sizeof(hmm_vec3),
    });

    // the resource bindings
    state.bind = (sg_bindings){
        .vertex_buffers = {
            [0] = geom_vbuf,
            [1] = inst_vbuf
        },
        .index_buffer = ibuf
    };

    // create a shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 300 es\n"
            "uniform mat4 mvp;\n"
            "in vec3 position;\n"
            "in vec4 color0;\n"
            "in vec3 instance_pos;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  vec4 pos = vec4(position + instance_pos, 1.0);"
            "  gl_Position = mvp * pos;\n"
            "  color = color0;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n",
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "color0",
            [2].glsl_name = "instance_pos"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
    });

    // pipeline state object, note the vertex attribute definition
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },
                [2] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_NONE
    });

    // hand off control to browser loop
    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

// draw one frame
static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    const float frame_time = 1.0f / 60.0f;

    // emit new particles
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (state.cur_num_particles < MAX_PARTICLES) {
            pos[state.cur_num_particles] = HMM_Vec3(0.0, 0.0, 0.0);
            vel[state.cur_num_particles] = HMM_Vec3(
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) * 0.5f + 2.0f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f);
            state.cur_num_particles++;
        }
        else {
            break;
        }
    }

    // update particle positions
    for (int i = 0; i < state.cur_num_particles; i++) {
        vel[i].Y -= 1.0f * frame_time;
        pos[i].X += vel[i].X * frame_time;
        pos[i].Y += vel[i].Y * frame_time;
        pos[i].Z += vel[i].Z * frame_time;
        // bounce back from 'ground'
        if (pos[i].Y < -2.0f) {
            pos[i].Y = -1.8f;
            vel[i].Y = -vel[i].Y;
            vel[i].X *= 0.8f; vel[i].Y *= 0.8f; vel[i].Z *= 0.8f;
        }
    }

    // update instance data
    sg_update_buffer(state.bind.vertex_buffers[1], &(sg_range){ pos, (size_t)state.cur_num_particles*sizeof(hmm_vec3) });

    // model-view-projection matrix
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)emsc_width()/(float)emsc_height(), 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.roty += 1.0f;
    const vs_params_t vs_params = {
        .mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(state.roty, HMM_Vec3(0.0f, 1.0f, 0.0f)))
    };

    // and the actual draw pass...
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = emsc_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.cur_num_particles);
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}
