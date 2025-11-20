//------------------------------------------------------------------------------
//  bufferoffsets-sapp.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "bufferoffsets-sapp.glsl.h"

static struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_pass_action pass_action;
    sg_pipeline pip;
} state = {
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f }
        }
    }
};

typedef struct {
    float x, y, r, g, b;
} vertex_t;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // a 2D triangle and quad in 1 vertex buffer and 1 index buffer
    vertex_t vertices[7] = {
        /* triangle */
        {  0.0f,   0.55f,  1.0f, 0.0f, 0.0f },
        {  0.25f,  0.05f,  0.0f, 1.0f, 0.0f },
        { -0.25f,  0.05f,  0.0f, 0.0f, 1.0f },

        /* quad */
        { -0.25f, -0.05f,  0.0f, 0.0f, 1.0f },
        {  0.25f, -0.05f,  0.0f, 1.0f, 0.0f },
        {  0.25f, -0.55f,  1.0f, 0.0f, 0.0f },
        { -0.25f, -0.55f,  1.0f, 1.0f, 0.0f }
    };
    uint16_t indices[9] = {
        0, 1, 2,
        0, 1, 2, 0, 2, 3
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "vertex-buffer",
    });
    state.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "index-buffer",
    });

    // a shader and pipeline to render 2D shapes
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(bufferoffsets_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [ATTR_bufferoffsets_position].format=SG_VERTEXFORMAT_FLOAT2,
                [ATTR_bufferoffsets_color0].format=SG_VERTEXFORMAT_FLOAT3
            }
        },
        .label = "pipeline",
    });
}

static void frame(void) {
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    // render the triangle (located at start of vertex- and index-buffer)
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
    });
    sg_draw(0, 3, 1);
    // render the quad (located after triangle data in vertex- and index-buffer)
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .vertex_buffer_offsets[0] = 3 * sizeof(vertex_t),
        .index_buffer = state.ibuf,
        .index_buffer_offset = 3 * sizeof(uint16_t),
    });
    sg_draw(0, 6, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
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
        .window_title = "Buffer Offsets (sokol-app)",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
