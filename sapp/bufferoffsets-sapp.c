//------------------------------------------------------------------------------
//  bufferoffsets-sapp.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"

static const char *vs_src, *fs_src;

typedef struct {
    float x, y, r, g, b;
} vertex_t;

static sg_pass_action pass_action = {
    .colors = {
        [0] = { .action=SG_ACTION_CLEAR, .val = { 0.5f, 0.5f, 1.0f, 1.0f } }
    }
};
static sg_pipeline pip;
static sg_bindings bind;

void init(void) {
    sg_setup(&(sg_desc){
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
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
    bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });
    bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* a shader and pipeline to render 2D shapes */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source = vs_src,
        .fs.source = fs_src
    });
    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0] = { .name="position", .sem_name="POS", .format=SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .name="color0", .sem_name="COLOR", .format=SG_VERTEXFORMAT_FLOAT3 }
            }
        }
    });
}

void frame(void) {
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(pip);
    /* render the triangle */
    bind.vertex_buffer_offsets[0] = 0;
    bind.index_buffer_offset = 0;
    sg_apply_bindings(&bind);
    sg_draw(0, 3, 1);
    /* render the quad */
    bind.vertex_buffer_offsets[0] = 3 * sizeof(vertex_t);
    bind.index_buffer_offset = 3 * sizeof(uint16_t);
    sg_apply_bindings(&bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "Buffer Offsets (sokol-app)",
    };
}

#if defined(SOKOL_GLCORE33)
static const char* vs_src =
    "#version 330\n"
    "in vec4 position;\n"
    "in vec4 color0;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "  gl_Position = position;\n"
    "  color = color0;\n"
    "}\n";
static const char* fs_src =
    "#version 330\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = color;\n"
    "}\n";
#elif defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
static const char* vs_src =
    "attribute vec4 position;\n"
    "attribute vec4 color0;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  gl_Position = position;\n"
    "  color = color0;\n"
    "}\n";
static const char* fs_src =
    "precision mediump float;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  gl_FragColor = color;\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char* vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct vs_in {\n"
    "  float4 position [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 position [[position]];\n"
    "  float4 color;\n"
    "};\n"
    "vertex vs_out _main(vs_in inp [[stage_in]]) {\n"
    "  vs_out outp;\n"
    "  outp.position = inp.position;\n"
    "  outp.color = inp.color;\n"
    "  return outp;\n"
    "}\n";
static const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "fragment float4 _main(float4 color [[stage_in]]) {\n"
    "  return color;\n"
    "};\n";
#elif defined(SOKOL_D3D11)
static const char* vs_src =
    "struct vs_in {\n"
    "  float4 pos: POS;\n"
    "  float4 color: COLOR;\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 color: COLOR0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = inp.pos;\n"
    "  outp.color = inp.color;\n"
    "  return outp;\n"
    "}\n";
static const char* fs_src =
    "float4 main(float4 color: COLOR0): SV_Target0 {\n"
    "  return color;\n"
    "}\n";
#endif
