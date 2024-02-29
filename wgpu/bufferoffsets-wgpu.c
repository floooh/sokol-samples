//------------------------------------------------------------------------------
//  bufferoffsets-wgpu.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
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
        .environment = wgpu_environment(),
        .logger.func = slog_func,
    });

    // a 2D triangle and quad in 1 vertex buffer and 1 index buffer
    const vertex_t vertices[7] = {
        // triangle
        {  0.0f,   0.55f,  1.0f, 0.0f, 0.0f },
        {  0.25f,  0.05f,  0.0f, 1.0f, 0.0f },
        { -0.25f,  0.05f,  0.0f, 0.0f, 1.0f },
        // quad
        { -0.25f, -0.05f,  0.0f, 0.0f, 1.0f },
        {  0.25f, -0.05f,  0.0f, 1.0f, 0.0f },
        {  0.25f, -0.55f,  1.0f, 0.0f, 0.0f },
        { -0.25f, -0.55f,  1.0f, 1.0f, 0.0f }
    };
    const uint16_t indices[9] = {
        0, 1, 2,
        0, 1, 2, 0, 2, 3
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    // a shader object with wgsl code
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) color: vec4f,\n"
            "}\n"
            "@vertex fn main(@location(0) pos: vec4f, @location(1) color: vec4f) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = pos;\n"
            "  out.color = color;\n"
            "  return out;\n"
            "}\n",
        .fs.source =
            "@fragment fn main(@location(0) color: vec4f) -> @location(0) vec4f {\n"
            "  return color;\n"
            "}\n",
    });

    // a shader and pipeline to render 2D shapes
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT2,
                [1].format=SG_VERTEXFORMAT_FLOAT3
            }
        }
    });
}

static void frame(void) {
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = wgpu_swapchain() });
    sg_apply_pipeline(state.pip);
    // render the triangle
    state.bind.vertex_buffer_offsets[0] = 0;
    state.bind.index_buffer_offset = 0;
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);
    // render the quad
    state.bind.vertex_buffer_offsets[0] = 3 * sizeof(vertex_t);
    state.bind.index_buffer_offset = 3 * sizeof(uint16_t);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .width = 640,
        .height = 480,
        .title = "bufferoffsets-wgpu"
    });
    return 0;
}
