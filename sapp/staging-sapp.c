//------------------------------------------------------------------------------
//  staging-sapp.c
//
//  Demonstrate uploading data into vertex- and index-buffer segments
//  via a write-transient staging buffer and a buffer-to-buffer GPU copy
//  operation.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#include "dbgui/dbgui.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "staging-sapp.glsl.h"

#define NUM_SEGMENTS (6)
#define MAX_SEGMENT_VERTICES (4096)
#define MAX_SEGMENT_INDICES (MAX_SEGMENT_VERTICES * 3)

static struct {
    sg_buffer vertex_buffer;
    sg_buffer index_buffer;
    sg_buffer staging_buffer;
    sg_pipeline pip;
    sg_pass_action pass_action;
    size_t vtx_segment_size;
    size_t idx_segment_size;
} state = {
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.1f, 0.1f, 0.15f, 1.0f },
        },
    },
};

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    const sshape_optional_components_t vtx_comps = { .colors = true };

    // a single 'staging buffer' for enough room
    state.vtx_segment_size = MAX_SEGMENT_VERTICES * sshape_vertex_size(&vtx_comps);
    state.idx_segment_size = MAX_SEGMENT_INDICES * sizeof(uint16_t);
    state.staging_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .write_transient = true,
            .copy_src = true,
        },
        .size = state.vtx_segment_size + state.idx_segment_size,
        .label = "staging-buffer",
    });

    // a vertex buffer with enough room for all dynamically updated 'segments'
    state.vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .vertex_buffer = true,  // technically not needed, since it's the default
            .copy_dst = true,
        },
        .size = NUM_SEGMENTS * state.vtx_segment_size,
        .label = "vertex-buffer",
    });

    // ...and an index buffer with the same 'segmentation'
    state.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .index_buffer = true,
            .copy_dst = true,
        },
        .size = NUM_SEGMENTS * state.idx_segment_size,
        .label = "index-buffer",
    });
}

static void frame(void) {
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
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
        .window_title = "staging-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
