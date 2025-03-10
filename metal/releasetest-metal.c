//------------------------------------------------------------------------------
//  release-metal.c
//  This is a modified triangle sample which creates and destroys all
//  resources in each frame. This tests whether
//  the delayed resource destruction works in the Metal backend
//  (a regression test for this bug: https://github.com/floooh/sokol/issues/32)
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
} state;

static void init(void) {
    // setup sokol, keep pool size small so the bug would trigger earlier
    sg_setup(&(sg_desc){
        .buffer_pool_size = 4,
        .image_pool_size = 2,
        .shader_pool_size = 4,
        .pipeline_pool_size = 4,
        .attachments_pool_size = 2,
        .environment = osx_environment(),
        .logger.func = slog_func,
    });
}

static void frame(void) {
    // create and destroy resource each frame, don't do this in real-world code!

    // a vertex buffer with 3 vertices
    float vertices[] = {
        // positions        colors
         0.0f, 0.5f, 0.5f,  1.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 1.0f
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // a shader pair, compiled from source code
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        /*
            The shader main() function cannot be called 'main' in
            the Metal shader languages, thus we define '_main' as the
            default function. This can be override with the
            sg_shader_desc.vs.entry and sg_shader_desc.fs.entry fields.
        */
        .vertex_func.source =
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
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "};\n"
    });

    // create a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 }
            },
        },
        .shader = shd
    });

    // draw one frame
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = osx_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();

    // release the resource we created above
    sg_destroy_buffer(state.bind.vertex_buffers[0]);
    sg_destroy_pipeline(state.pip);
    sg_destroy_shader(shd);
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 1, SG_PIXELFORMAT_DEPTH_STENCIL, "releasetest-metal", init, frame, shutdown);
    return 0;
}
