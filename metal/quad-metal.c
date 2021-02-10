//------------------------------------------------------------------------------
//  quad-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
} state;

static void init(void) {
    /* setup sokol */
    sg_setup(&(sg_desc){
        .context = osx_get_context()
    });

    /* a vertex buffer */
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,        
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    /* an index buffer with 2 triangles */
    const uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    /* a shader (use separate shader sources here */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct vs_in {\n"
            "  float4 position [[attribute(0)]];\n"
            "  float4 color [[attribute(1)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 position [[position]];\n"
            "  float4 color [[user(usr0)]];\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]]) {\n"
            "  vs_out out;\n"
            "  out.position = in.position;\n"
            "  out.color = in.color;\n"
            "  return out;\n"
            "}\n",
        .fs.source =
            "#include <metal_stdlib>\n"
            "#include <simd/simd.h>\n"
            "using namespace metal;\n"
            "struct fs_in {\n"
            "  float4 color [[user(usr0)]];\n"
            "};\n"
            "fragment float4 _main(fs_in in [[stage_in]]) {\n"
            "  return in.color;\n"
            "};\n"
    });

    /* a pipeline state object */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            /* test to provide attr offsets, but no buffer stride, this should compute the stride */
            .attrs = {
                [0] = { .offset=0, .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset=12, .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        }
    });
}

static void frame(void) {
    sg_begin_default_pass(&state.pass_action, osx_width(), osx_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 1, "Sokol Quad (Metal)", init, frame, shutdown);
    return 0;
}
