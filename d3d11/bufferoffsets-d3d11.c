//------------------------------------------------------------------------------
//  bufferoffsets-glfw.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_D3D11_SHADER_COMPILER
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"

typedef struct {
    float x, y, r, g, b;
} vertex_t;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    /* setup d3d11 app wrapper and sokol_gfx */
    const int msaa_samples = 4;
    const int width = 800;
    const int height = 600;
    d3d11_init(width, height, msaa_samples, L"Sokol Buffer Offsets D3D11");
    sg_setup(&(sg_desc){
        .d3d11_device = d3d11_device(),
        .d3d11_device_context = d3d11_device_context(),
        .d3d11_render_target_view_cb = d3d11_render_target_view,
        .d3d11_depth_stencil_view_cb = d3d11_depth_stencil_view
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
    sg_buffer vb = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });
    sg_buffer ib = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* a shader to render the 2D shapes */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "struct vs_in {\n"
            "  float2 pos: POSITION;\n"
            "  float3 color: COLOR0;\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 color: COLOR0;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = float4(inp.pos, 0.5, 1.0);\n"
            "  outp.color = float4(inp.color, 1.0);\n"
            "  return outp;\n"
            "}\n",
        .fs.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
            "}\n"
    });

    /* a pipeline state object, default states are fine */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0] = { .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .sem_name="COLOR", .format=SG_VERTEXFORMAT_FLOAT3 }
            }
        }
    });

    /* a draw state object with the resource bindings, before drawing,
       the buffer offsets will be updates
    */
    sg_draw_state ds = {
        .pipeline = pip,
        .vertex_buffers[0] = vb,
        .index_buffer = ib
    };

    /* a pass action to clear to blue-ish */
    sg_pass_action pass_action = {
        .colors = {
            [0] = { .action=SG_ACTION_CLEAR, .val = { 0.5f, 0.5f, 1.0f, 1.0f } }
        }
    };

    while (d3d11_process_events()) {
        sg_begin_default_pass(&pass_action, d3d11_width(), d3d11_height());
        /* render the triangle */
        ds.vertex_buffer_offsets[0] = 0;
        ds.index_buffer_offset = 0;
        sg_apply_draw_state(&ds);
        sg_draw(0, 3, 1);
        /* render the quad from the same vertex- and index-buffer */
        ds.vertex_buffer_offsets[0] = 3 * sizeof(vertex_t);
        ds.index_buffer_offset = 3 * sizeof(uint16_t);
        sg_apply_draw_state(&ds);
        sg_draw(0, 6, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }

    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
