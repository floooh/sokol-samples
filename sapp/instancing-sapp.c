//------------------------------------------------------------------------------
//  instancing.c
//  Demonstrate simple hardware-instancing using a static geometry buffer
//  and a dynamic instance-data buffer.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath.h"
#include "dbgui/dbgui.h"
#include "instancing-sapp.glsl.h"

#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float ry;
    int cur_num_particles;
    vec3_t pos[MAX_PARTICLES];
    vec3_t vel[MAX_PARTICLES];
} state;

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // a pass action for the default render pass
    state.pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };

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
        .data = SG_RANGE(vertices),
        .label = "geometry-vertices"
    });

    // index buffer for static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "geometry-indices"
    });

    // empty, dynamic instance-data vertex buffer, goes into vertex-buffer-slot 1
    state.bind.vertex_buffers[1] = sg_make_buffer(&(sg_buffer_desc){
        .size = MAX_PARTICLES * sizeof(vec3_t),
        .usage.stream_update = true,
        .label = "instance-data"
    });

    // a shader
    sg_shader shd = sg_make_shader(instancing_shader_desc(sg_query_backend()));

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // vertex buffer at slot 1 must step per instance
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [ATTR_instancing_pos]      = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },
                [ATTR_instancing_color0]   = { .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },
                [ATTR_instancing_inst_pos] = { .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "instancing-pipeline"
    });
}

void frame(void) {
    const float frame_time = (float)(sapp_frame_duration());

    // emit new particles
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (state.cur_num_particles < MAX_PARTICLES) {
            state.pos[state.cur_num_particles] = vec3(0.0, 0.0, 0.0);
            state.vel[state.cur_num_particles] = vec3(
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f,
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f,
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f);
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
    sg_update_buffer(state.bind.vertex_buffers[1], &(sg_range){
        .ptr = state.pos,
        .size = (size_t)state.cur_num_particles * sizeof(vec3_t)
    });

    // model-view-projection matrix
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), sapp_widthf()/sapp_heightf(), 0.01f, 50.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 8.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    state.ry += 60.0f * frame_time;
    const vs_params_t vs_params = { .mvp = vm_mul(mat44_rotation_y(vm_radians(state.ry)), view_proj) };

    // ...and draw
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.cur_num_particles);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "Instancing (sokol-app)",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
