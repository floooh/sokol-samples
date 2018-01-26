//------------------------------------------------------------------------------
//  triangle-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"

sg_pass_action pass_action = {0};
sg_draw_state draw_state = {0};

void init(const void* mtl_device) {
    /* setup sokol */
    sg_desc desc = {
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    };
    sg_setup(&desc);

    /* a vertex buffer with 3 vertices */
    float vertices[] = {
        // positions        colors
         0.0f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f
    };
    draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* a shader pair, compiled from source code */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        /*
            The shader main() function cannot be called 'main' in 
            the Metal shader languages, thus we define '_main' as the
            default function. This can be override with the 
            sg_shader_desc.vs.entry and sg_shader_desc.fs.entry fields.
        */
        .vs.source =
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
            "}\n",
        .fs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "};\n"
    });

    /* create a pipeline object */
    draw_state.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        /* Metal has explicit attribute locations, and the vertex layout
           has no gaps, so we don't need to provide stride, offsets
           or attribute names
        */
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 }
            },
        },
        .shader = shd
    });
}

void frame() {
    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
    sg_apply_draw_state(&draw_state);
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 1, "Sokol Triangle (Metal)", init, frame, shutdown);
    return 0;
}
