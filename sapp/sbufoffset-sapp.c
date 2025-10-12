//------------------------------------------------------------------------------
//  sbufoffset-sapp.c
//
//  An artificially convoluted sample to test binding storage buffers with
//  offset:
//
//  - uses a single storage buffer to hold both index- and vertex-data
//    (first the index data, followed by vertex data)
//  - uses two separate compute shaders to initialize the index- and vertex-data
//    for the vertex data the storage buffer is bound with an offset
//  - uses 32-bit indices for simplicity (since this is the smallest
//    storage buffer 'item size')
//  - for rendering the index buffer is bound traditionally
//  - and the vertex data is bound as storage buffer with offset and
//    vertex shader using vertex-pulling
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
#include "sbufoffset-sapp.glsl.h"

static struct {
    float rx, ry;
    sg_buffer buf;
    sg_view vtx_view;
    sg_pipeline pip;
    sg_pass_action pass_action;
} state;

// number of vertices and indices in a cube
#define NUM_VERTICES (24)
#define NUM_INDICES (36)

static void draw_fallback(void);
static vs_params_t compute_vsparams(void);

static size_t roundup(size_t val, size_t round_to) {
    return (val+(round_to-1)) & ~(round_to-1);
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // check for compute support and render fallback via sokol_debugtext.h if not available
    if (!sg_query_features().compute) {
        sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_cpc(), .logger.func = slog_func });
        // set pass action to render red background
        state.pass_action = (sg_pass_action){
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1, 0, 0, 1 } },
        };
        return;
    }

    // setup pass action to clear to the actual background color
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.25f, 0.375, 0.125f, 1.0f } },
    };

    // create a buffer with index- and storage-buffer usage which holds both the index- and vertex-data
    // NOTE: storage buffer view offsets must be aligned to 256 bytes
    const size_t vertices_offset = roundup(NUM_INDICES * sizeof(uint32_t), 256);
    state.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .index_buffer = true,
            .storage_buffer = true,
        },
        .size = vertices_offset + NUM_VERTICES * sizeof(sb_vertex_t),
        .label = "storage-buffer",
    });

    // two storage buffer views, one pointing to the index data block and the other to the vertex data block
    sg_view idx_view = sg_make_view(&(sg_view_desc){
        .storage_buffer = {
            .buffer = state.buf,
            .offset = 0,
        },
        .label = "index-view"
    });
    state.vtx_view = sg_make_view(&(sg_view_desc){
        .storage_buffer = {
            .buffer = state.buf,
            .offset = (int)vertices_offset,
        },
        .label = "vertex-view",
    });

    // create compute pipeline objects
    sg_pipeline idx_init_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(init_indices_shader_desc(sg_query_backend())),
        .label = "init-indices-pipeline",
    });
    sg_pipeline vtx_init_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(init_vertices_shader_desc(sg_query_backend())),
        .label = "init-vertices-pipeline",
    });

    // a render pipeline object (note: 32-bit indices, and no vertex layout since we use vertex pulling)
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(sbufoffset_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT32,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .label = "render-pipeline",
    });

    // one-time init of the index- and vertex-data via compute shader
    // (NOTE: this code runs as part of the first frame, so no extra sg_commit() needed)
    sg_begin_pass(&(sg_pass){ .compute = true });
    sg_apply_pipeline(idx_init_pip);
    sg_apply_bindings(&(sg_bindings){ .views[VIEW_cs_idx_ssbo] = idx_view });
    sg_dispatch(((NUM_INDICES - 1) / 32) + 1, 1, 1);
    sg_apply_pipeline(vtx_init_pip);
    sg_apply_bindings(&(sg_bindings){ .views[VIEW_cs_vtx_ssbo] = state.vtx_view });
    sg_dispatch(((NUM_VERTICES - 1) / 32) + 1, 1, 1);
    sg_end_pass();

    // get rid of any objects we don't need anymore
    sg_destroy_view(idx_view);
    sg_destroy_pipeline(idx_init_pip);
    sg_destroy_pipeline(vtx_init_pip);
}

static void frame(void) {
    if (!sg_query_features().compute) {
        draw_fallback();
        return;
    }
    const vs_params_t vs_params = compute_vsparams();

    // draw cube with both indices and vertices provided by the same buffer,
    // and using vertex pulling in the vertex shader
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings){
        .index_buffer = state.buf,
        .index_buffer_offset = 0,
        .views[VIEW_vs_vtx_ssbo] = state.vtx_view,
    });
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, NUM_INDICES, 1);
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

static vs_params_t compute_vsparams(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const float t = (float)(sapp_frame_duration() * 60.0);
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const mat44_t rxm = mat44_rotation_x(vm_radians(state.rx));
    const mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    const mat44_t model = vm_mul(rym, rxm);
    return (vs_params_t){ .mvp = vm_mul(model, view_proj) };
}

static void draw_fallback(void) {
    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_pos(1, 1);
    sdtx_puts("COMPUTE SHADERS NOT SUPPORTED ON THIS BACKEND");
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    sdtx_draw();
    sg_end_pass();
    sg_commit();
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
        .window_title = "sbufoffset-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
