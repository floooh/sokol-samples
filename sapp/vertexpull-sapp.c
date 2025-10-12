//------------------------------------------------------------------------------
//  vertexpull-sapp.c
//
//  Demonstrates vertex pulling from a storage buffer.
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
#include "vertexpull-sapp.glsl.h"

static struct {
    float rx, ry;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
} state;

static vs_params_t compute_vsparams(float rx, float ry);
static void draw_fallback(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // if storage buffers are not supported on this platform, render
    // a red screen and error message via sokol-debugtext
    if (!sg_query_features().compute) {
        sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_cpc() });
        return;
    }

    // a storage buffer with the cube vertex data
    sb_vertex_t vertices[] = {
        { .pos = { -1.0, -1.0, -1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = { -1.0, -1.0, -1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0 }, .color = { 1.0, 0.5, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0 }, .color = { 1.0, 0.5, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0 }, .color = { 1.0, 0.5, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0 }, .color = { 1.0, 0.5, 0.0, 1.0 } },
        { .pos = { -1.0, -1.0, -1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
    };
    sg_buffer sbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .data = SG_RANGE(vertices),
        .label = "cube-vertices",
    });

    // an index buffer with the cube indices
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
        .data = SG_RANGE(indices),
        .label = "cube-indices",
    });

    // a pipeline object, note that there is no vertex layout
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(vertexpull_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .label = "cube-pipeline"
    });

    // resource bindings, note that there is no vertex buffer binding
    state.bind = (sg_bindings){
        .index_buffer = ibuf,
        .views[VIEW_ssbo] = sg_make_view(&(sg_view_desc){
            .storage_buffer = { .buffer = sbuf, .offset = 0 },
            .label = "cube-vertices-view",
        }),
    };

    // define a clear color
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.75f, 0.5f, 0.25f, 1.0f } },
    };
}

static void frame(void) {
    if (!sg_query_features().compute) {
        draw_fallback();
        return;
    }
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const vs_params_t vs_params = compute_vsparams(state.rx, state.ry);

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void draw_fallback(void) {
    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_pos(1, 1);
    sdtx_puts("STORAGE BUFFERS NOT SUPPORTED");
    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1, 0, 0, 1} },
        },
        .swapchain = sglue_swapchain_next()
    });
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

static vs_params_t compute_vsparams(float rx, float ry) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return (vs_params_t){ .mvp = vm_mul(model, view_proj) };
}

static void cleanup(void) {
    __dbgui_shutdown();
    if (!sg_query_features().compute) {
        sdtx_shutdown();
    }
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "vertexpull-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
