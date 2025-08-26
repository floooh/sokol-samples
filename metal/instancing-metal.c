//------------------------------------------------------------------------------
//  instancing-metal.c
//------------------------------------------------------------------------------
#include "stdlib.h"
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

#define WIDTH (640)
#define HEIGHT (480)
#define SAMPLE_COUNT (4)
#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float ry;
    // particle positions and velocity
    int cur_num_particles;
    vec3_t pos[MAX_PARTICLES];
    vec3_t vel[MAX_PARTICLES];
} state = {
    // a pass-action to clear to black
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    }
};

typedef struct {
    mat44_t mvp;
} vs_params_t;

static void init(void) {
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    // vertex buffer for static geometry, goes into vertex-buffer-slot 0
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
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // index buffer for static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // empty, dynamic instance-data vertex buffer, goes into vertex-buffer-slot 1
    state.bind.vertex_buffers[1] = sg_make_buffer(&(sg_buffer_desc){
        .usage.stream_update = true,
        .size = MAX_PARTICLES * sizeof(vec3_t),
    });

    // a shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float3 pos [[attribute(0)]];\n"
            "  float4 color [[attribute(1)]];\n"
            "  float3 instance_pos [[attribute(2)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float4 color;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  float4 pos = float4(in.pos + in.instance_pos, 1.0);\n"
            "  out.pos = params.mvp * pos;\n"
            "  out.color = in.color;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
    });

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // vertex buffer at slot 1 must step per instance
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },  // position
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },  // color
                [2] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 },  // instance_pos
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

}

static void frame(void) {
    const float frame_time = 1.0f / 60.0f;

    // emit new particles
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (state.cur_num_particles < MAX_PARTICLES) {
            state.pos[state.cur_num_particles] = vec3(0.0, 0.0, 0.0);
            state.vel[state.cur_num_particles] = vec3(
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) * 0.5f + 2.0f,
                ((float)(rand() & 0xFFFF) / 0xFFFF) - 0.5f);
            state.cur_num_particles++;
        } else {
            break;
        }
    }

    // update particle positions
    for (int i = 0; i < state.cur_num_particles; i++) {
        state.vel[i].y -= 1.0f * frame_time;
        state.pos[i].x += state.vel[i].x * frame_time;
        state.pos[i].y += state.vel[i].y * frame_time;
        state.pos[i].z += state.vel[i].z * frame_time;
        // bounce back from 'ground'
        if (state.pos[i].y < -2.0f) {
            state.pos[i].y = -1.8f;
            state.vel[i].y = -state.vel[i].y;
            state.vel[i].x *= 0.8f; state.vel[i].y *= 0.8f; state.vel[i].z *= 0.8f;
        }
    }

    // update instance data
    sg_update_buffer(state.bind.vertex_buffers[1], &(sg_range) {
        .ptr = state.pos,
        .size = (size_t)state.cur_num_particles * sizeof(vec3_t)
    });

    // model-view-projection matrix
    state.ry += 1.0f;
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)osx_width()/(float)osx_height(), 0.01f, 50.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 8.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t model = mat44_rotation_y(vm_radians(state.ry));
    const vs_params_t vs_params = { .mvp = vm_mul(model, vm_mul(view, proj)) };

    // ...and draw
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = osx_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.cur_num_particles);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, SAMPLE_COUNT, SG_PIXELFORMAT_DEPTH, "instancing-metal", init, frame, shutdown);
    return 0;
}
