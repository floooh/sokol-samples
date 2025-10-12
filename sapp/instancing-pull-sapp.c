//------------------------------------------------------------------------------
//  instancing-pull-sapp.c
//
//  Same as instancing-sapp.c, but pull both vertex and instance data
//  from storage buffers.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "dbgui/dbgui.h"
#include "instancing-pull-sapp.glsl.h"

#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    sg_pass_action pass_action;
    sg_buffer inst_buf;
    sg_pipeline pip;
    sg_bindings bind;
    float ry;
    int cur_num_particles;
    sb_instance_t inst[MAX_PARTICLES];
    vec3_t vel[MAX_PARTICLES];
} state;

static void draw_fallback(void);
static void emit_particles(void);
static void update_particles(float frame_time);
static vs_params_t compute_vsparams(float frame_time);

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // storage buffers are not supported on the current backend?
    // (in this case a red screen and an error message is rendered)
    if (!sg_query_features().compute) {
        sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_cpc() });
        return;
    }

    // a pass action for the default render pass
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.1f, 0.2f, 1.0f } }
    };

    // a storage buffer and view for the static geometry
    const float r = 0.05f;
    const sb_vertex_t vertices[] = {
        { .pos = vec3(0.0f,   -r, 0.0f), .color = vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { .pos = vec3(   r, 0.0f, r   ), .color = vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { .pos = vec3(   r, 0.0f, -r  ), .color = vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { .pos = vec3(  -r, 0.0f, -r  ), .color = vec4(1.0f, 1.0f, 0.0f, 1.0f) },
        { .pos = vec3(  -r, 0.0f, r   ), .color = vec4(0.0f, 1.0f, 1.0f, 1.0f) },
        { .pos = vec3(0.0f,    r, 0.0f), .color = vec4(1.0f, 0.0f, 1.0f, 1.0f) },
    };
    sg_buffer sbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .data = SG_RANGE(vertices),
        .label = "geometry-vertices",
    });
    state.bind.views[VIEW_vertices] = sg_make_view(&(sg_view_desc){
        .storage_buffer = { .buffer = sbuf },
        .label = "geometry-vertices-view",
    });

    // an index buffer for the static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "geometry-indices"
    });

    // a dynamic storage buffer for the per-instance data
    state.inst_buf = sg_make_buffer(&(sg_buffer_desc){
        .usage = { .storage_buffer = true, .stream_update = true },
        .size = MAX_PARTICLES * sizeof(sb_instance_t),
        .label = "instance-data",
    });
    state.bind.views[VIEW_instances] = sg_make_view(&(sg_view_desc){
        .storage_buffer = { .buffer = state.inst_buf },
        .label = "instance-data-view",
    });

    // a shader and pipeline object, note the lack of a vertex layout definition
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(instancing_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "instancing-pipeline",
    });
}

static void frame(void) {
    if (!sg_query_features().compute) {
        draw_fallback();
        return;
    }

    const float frame_time = (float)sapp_frame_duration();

    // emit new particles, and update particle positions
    emit_particles();
    update_particles(frame_time);

    // update instance data storage buffer
    sg_update_buffer(state.inst_buf, &(sg_range){
        .ptr = state.inst,
        .size = (size_t)state.cur_num_particles * sizeof(sb_instance_t),
    });

    // compute model-view-projection matrix
    const vs_params_t vs_params = compute_vsparams(frame_time);

    // ...and draw
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.cur_num_particles);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    if (!sg_query_features().compute) {
        sdtx_shutdown();
    }
    sg_shutdown();
}

static void draw_fallback(void) {
    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_pos(1, 1);
    sdtx_puts("STORAGE BUFFERS NOT SUPPORTED");
    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } },
        },
        .swapchain = sglue_swapchain_next()
    });
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

static void emit_particles(void) {
    for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
        if (state.cur_num_particles < MAX_PARTICLES) {
            state.inst[state.cur_num_particles].pos = vec3(0.0, 0.0, 0.0);
            state.vel[state.cur_num_particles] = vec3(
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f,
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f,
                ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f);
            state.cur_num_particles++;
        } else {
            break;
        }
    }
}

static void update_particles(float frame_time) {
    for (int i = 0; i < state.cur_num_particles; i++) {
        state.vel[i].y -= 1.0f * frame_time;
        state.inst[i].pos.x += state.vel[i].x * frame_time;
        state.inst[i].pos.y += state.vel[i].y * frame_time;
        state.inst[i].pos.z += state.vel[i].z * frame_time;
        // bounce back from 'ground'
        if (state.inst[i].pos.y < -2.0f) {
            state.inst[i].pos.y = -1.8f;
            state.vel[i].y = -state.vel[i].y;
            state.vel[i].x *= 0.8f; state.vel[i].y *= 0.8f; state.vel[i].z *= 0.8f;
        }
    }
}

static vs_params_t compute_vsparams(float frame_time) {
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), sapp_widthf()/sapp_heightf(), 0.01f, 50.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 8.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    state.ry += 60.0f * frame_time;
    return (vs_params_t) {
        .mvp = vm_mul(mat44_rotation_y(vm_radians(state.ry)), view_proj),
    };
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
        .window_title = "instancing-pull-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
