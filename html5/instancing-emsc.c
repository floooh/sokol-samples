//------------------------------------------------------------------------------
//  instancing-emsc.c
//------------------------------------------------------------------------------
#include <stddef.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"

const int WIDTH = 640;
const int HEIGHT = 480;
const int MAX_PARTICLES = 512 * 1024;
const int NUM_PARTICLES_EMITTED_PER_FRAME = 10;

sg_draw_state draw_state;
sg_pass_action pass_action;
float roty = 0.0f;
hmm_mat4 view_proj;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

/* particle positions and velocity */
int cur_num_particles = 0;
hmm_vec3 pos[MAX_PARTICLES];
hmm_vec3 vel[MAX_PARTICLES];

void draw();

int main() {
    /* setup WebGL context */
    emscripten_set_canvas_size(WIDTH, HEIGHT);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    ctx = emscripten_webgl_create_context(0, &attrs);
    emscripten_webgl_make_context_current(ctx);

    /* setup sokol_gfx */
    sg_desc desc;
    sg_init_desc(&desc);
    sg_setup(&desc);
    assert(sg_isvalid());
    
    sg_init_draw_state(&draw_state);
    sg_init_pass_action(&pass_action);
    pass_action.color[0][0] = 0.0f;
    pass_action.color[0][1] = 0.0f;
    pass_action.color[0][2] = 0.0f;
    
    /* vertex buffer for static geometry (goes into vertex buffer bind slot 0) */
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
    sg_buffer_desc buf_desc;
    sg_init_buffer_desc(&buf_desc);
    buf_desc.size = sizeof(vertices);
    buf_desc.data_ptr = vertices;
    buf_desc.data_size = sizeof(vertices);
    draw_state.vertex_buffers[0] = sg_make_buffer(&buf_desc);

    /* index buffer for static geometry */
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    sg_init_buffer_desc(&buf_desc);
    buf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    buf_desc.size = sizeof(indices);
    buf_desc.data_ptr = indices;
    buf_desc.data_size = sizeof(indices);
    draw_state.index_buffer = sg_make_buffer(&buf_desc);
    
    /* empty, dynamic instance-data vertex buffer (goes into vertex buffer bind slot 1) */
    sg_init_buffer_desc(&buf_desc);
    buf_desc.size = MAX_PARTICLES * sizeof(hmm_vec4);
    buf_desc.usage = SG_USAGE_STREAM;
    draw_state.vertex_buffers[1] = sg_make_buffer(&buf_desc);

    /* create a shader */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(vs_params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "mvp", offsetof(vs_params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    shd_desc.vs.source =
        "uniform mat4 mvp;\n"
        "attribute vec3 position;\n"
        "attribute vec4 color0;\n"
        "attribute vec3 instance_pos;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  vec4 pos = vec4(position + instance_pos, 1.0);"
        "  gl_Position = mvp * pos;\n"
        "  color = color0;\n"
        "}\n";
    shd_desc.fs.source =
        "precision mediump float;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_FragColor = color;\n"
        "}\n";
    sg_shader shd = sg_make_shader(&shd_desc);

    /* pipeline state object, note the vertex attribute definition */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 28);
    sg_init_vertex_stride(&pip_desc, 1, 12);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    sg_init_named_vertex_attr(&pip_desc, 1, "instance_pos", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_vertex_step(&pip_desc, 1, SG_VERTEXSTEP_PER_INSTANCE, 1);
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.rast.cull_mode = SG_CULLMODE_BACK;
    draw_state.pipeline = sg_make_pipeline(&pip_desc);

    /* default pass action (clear to grey) */
    sg_pass_action pass_action;
    sg_init_pass_action(&pass_action);

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 50.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);

    /* hand off control to browser loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

/* draw one frame */ 
void draw() {
    const float frame_time = 1.0f / 60.0f;

    /* emit new particles */
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (cur_num_particles < MAX_PARTICLES) {
            pos[cur_num_particles] = HMM_Vec3(0.0, 0.0, 0.0);
            vel[cur_num_particles] = HMM_Vec3(
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) * 0.5f + 2.0f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f);
            cur_num_particles++;
        }
        else {
            break;
        }
    }

    /* update particle positions */
    for (int i = 0; i < cur_num_particles; i++) {
        vel[i].Y -= 1.0f * frame_time;
        pos[i].X += vel[i].X * frame_time;
        pos[i].Y += vel[i].Y * frame_time;
        pos[i].Z += vel[i].Z * frame_time;
        /* bounce back from 'ground' */
        if (pos[i].Y < -2.0f) {
            pos[i].Y = -1.8f;
            vel[i].Y = -vel[i].Y;
            vel[i].X *= 0.8f; vel[i].Y *= 0.8f; vel[i].Z *= 0.8f;
        }
    }

    /* update instance data */
    sg_update_buffer(draw_state.vertex_buffers[1], pos, cur_num_particles*sizeof(hmm_vec3));

    /* model-view-projection matrix */
    roty += 1.0f;
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(roty, HMM_Vec3(0.0f, 1.0f, 0.0f)));;

    /* and the actual draw pass... */
    sg_begin_default_pass(&pass_action, WIDTH, HEIGHT);
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 24, cur_num_particles);
    sg_end_pass();
    sg_commit();
}
