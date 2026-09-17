//------------------------------------------------------------------------------
//  scroller-sapp.c
//
//  An old-school text scroller effect.
//
//  Demonstrates uploading snippets of persistent data into a vertex
//  buffer, using a write-transient/copy-src staging buffer and an
//  immutable/copy-dst vertex buffer. The vertex buffer is updated like
//  a ring buffer whenever a character is entering view on the right screen
//  edge and and old character disappears on the left screen edge.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
// sokol_debugtext.h is only used for the embedded font data
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"
#include "scroller-sapp.glsl.h"
#include <stdlib.h> // malloc/free
#include <assert.h> // assert

#define MAX_CHARS (16)     // max characters visible at the same time
#define CHAR_DIM (8)       // width/height of a character
#define CHAR_QUADS (CHAR_DIM * CHAR_DIM)
#define MAX_CHAR_VERTICES ((CHAR_DIM+1)*(CHAR_DIM+1))
#define MAX_CHAR_INDICES (CHAR_QUADS * 6)
#define MAX_SCROLLER_VERTICES (MAX_CHARS * MAX_CHAR_VERTICES)
#define MAX_SCROLLER_INDICES (MAX_CHARS * MAX_CHAR_INDICES)

static struct {
    sg_buffer transfer_buffer;  // small transfer buffer for one character mesh
    sg_buffer ring_buffer;      // vertex buffer with room for all visible char meshes
    sg_buffer index_buffer;     // static-content index buffer with triangle indices
    sg_pipeline pip;
    sg_pass_action pass_action;
} state = {
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.082f, 0.39f, 0.75f, 1.0f }
        }
    }
};

static void populate_index_buffer(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    // a small write-transient transfer buffer with room for vertices of one character
    state.transfer_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .write_transient = true,
            .copy_src = true,
        },
        .size = MAX_CHAR_VERTICES,
        .label = "transfer-buffer",
    });

    // a vertex buffer for the scroller text, updated like a ring buffer
    // this is updated piece-wise from the transfer buffer whenever a new
    // character becomes visible
    state.ring_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .vertex_buffer = true,
            .copy_dst = true,
        },
        .size = MAX_SCROLLER_VERTICES,
        .label = "scroll-vertex-buffer",
    });

    // an index buffer with static content, populated with triangle indices into the
    // ring vertex buffer
    state.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .index_buffer = true,
            .write_unsealed = true,
        },
        .size = MAX_SCROLLER_INDICES * sizeof(uint16_t),
        .label = "scroll-index-buffer",
    });
    populate_index_buffer();
    sg_seal_buffer(state.index_buffer);

    // a pipeline object for simple 2D rendering
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(scroller_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_scroller_position].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .label = "scroll-pipeline",
    });

}

static void frame(void) {
    sg_begin_pass(&(sg_pass){
        .action = state.pass_action,
        .swapchain = sglue_swapchain(),
    });
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

static void populate_index_buffer(void) {
    const size_t size = MAX_SCROLLER_INDICES * sizeof(uint16_t);
    uint16_t* ptr = malloc(size);
    int dst_index = 0;
    uint16_t base_vertex = 0;
    for (int char_index = 0; char_index < MAX_CHARS; char_index++) {
        for (uint16_t j = 0; j < CHAR_DIM; j++) {
            for (uint16_t i = 0; i < CHAR_DIM; i++) {
                const uint16_t i0 = (j * (CHAR_DIM + 1)) + i;
                const uint16_t i1 = i0 + 1;
                const uint16_t i2 = i0 + (CHAR_DIM + 1);
                const uint16_t i3 = i2 + 1;
                // first quad triangle
                ptr[dst_index++] = base_vertex + i0;
                ptr[dst_index++] = base_vertex + i1;
                ptr[dst_index++] = base_vertex + i3;
                // second quad triangle
                ptr[dst_index++] = base_vertex + i0;
                ptr[dst_index++] = base_vertex + i3;
                ptr[dst_index++] = base_vertex + i2;
            }
        }
        base_vertex += MAX_CHAR_VERTICES;
    }
    assert(base_vertex == MAX_SCROLLER_VERTICES);

    sg_write_buffer_unsealed(&(sg_write_buffer_desc){
        .src.data = { .ptr = ptr, .size = size },
        .dst.buffer = state.index_buffer,
    });

    free(ptr);
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
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .window_title = "scroller-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
