//------------------------------------------------------------------------------
//  triangle-d3d11.c
//  Canonical triangle sample for sokol_gfx with D3D11 backend.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_gfx.h"
#include "sokol_log.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d11 app wrapper
    d3d11_init(&(d3d11_desc_t){
        .width = 640,
        .height = 480,
        .no_depth_buffer = true,
        .title = L"triangle-d3d11.c"
    });

    // setup sokol gfx
    sg_setup(&(sg_desc){
        .environment = d3d11_environment(),
        .logger.func = slog_func,
    });

    // default pass action (clear to grey)
    sg_pass_action pass_action = { 0 };

    // a vertex buffer with the triangle vertices
    const float vertices[] = {
        // positions            colors
         0.0f, 0.5f, 0.5f,      1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // define resource bindings
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf
    };

    // a shader to render the triangle
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].sem_name = "POS",
            [1].sem_name = "COLOR"
        },
        .vs.source =
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
            "}\n",
        .fs.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
            "}\n"
    });

    // a pipeline object
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        // if the vertex layout doesn't have gaps, don't need to provide strides and offsets
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = shd
    });

    // the draw loop
    while (d3d11_process_events()) {
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = d3d11_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_draw(0, 3, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    // shutdown everything
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
