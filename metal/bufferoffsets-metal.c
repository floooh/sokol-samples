//------------------------------------------------------------------------------
//  bufferoffsets-metal.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"

typedef struct {
    float x, y, r, g, b;
} vertex_t;

sg_pass_action pass_action = {
    .colors = {
        [0] = { .action=SG_ACTION_CLEAR, .val = { 0.5f, 0.5f, 1.0f, 1.0f } }
    }
};
sg_draw_state ds = { 0 };

void init(const void* mtl_device) {
    sg_setup(&(sg_desc){
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    });

    /* a 2D triangle and quad in 1 vertex buffer and 1 index buffer */
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
    ds.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });
    ds.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* a shader and pipeline to render 2D shapes */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct vs_in {\n"
            "  float2 pos [[attribute(0)]];\n"
            "  float3 color [[attribute(1)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float4 color;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]]) {\n"
            "  vs_out out;\n"
            "  out.pos = float4(in.pos, 0.5f, 1.0f);\n"
            "  out.color = float4(in.color, 1.0);\n"
            "  return out;\n"
            "}\n",
        .fs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "}\n"
    });

    ds.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT2,
                [1].format = SG_VERTEXFORMAT_FLOAT3
            }
        }
    });
}

void frame() {
    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
    /* render the triangle */
    ds.vertex_buffer_offsets[0] = 0;
    ds.index_buffer_offset = 0;
    sg_apply_draw_state(&ds);
    sg_draw(0, 3, 1);
    /* render the quad */
    ds.vertex_buffer_offsets[0] = 3 * sizeof(vertex_t);
    ds.index_buffer_offset = 3 * sizeof(uint16_t);
    sg_apply_draw_state(&ds);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();

}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 1, "Sokol Buffer Offsets (Metal)", init, frame, shutdown);
    return 0;
}
