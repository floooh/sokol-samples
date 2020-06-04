//------------------------------------------------------------------------------
//  quad-d3d11.c
//  Render a quad with vertex indices.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    /* setup d3d11 app wrapper and sokol_gfx */
    d3d11_init(640, 480, 1, L"Sokol Quad D3D11");
    sg_setup(&(sg_desc){
        .context = d3d11_get_context()
    });

    /* vertex and index buffer */
    const float vertices[] = {
        /* positions            colors */
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
    };
    const uint16_t indices[] = {
        0, 1, 2,    /* first triangle */
        0, 2, 3,    /* second triangle */
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* a shader to render the quad */
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

    /* pipeline state object */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            /* test to provide attr offsets, but no buffer stride, this should compute the stride */
            .attrs = {
                /* vertex attrs can also be bound by location instead of name (but not in GLES2) */
                [0] = { .offset=0, .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset=12, .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        }
    });

    /* resource bindings */
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* a default pass action */
    sg_pass_action pass_action = { 0 };

    /* draw loop */
    while (d3d11_process_events()) {
        sg_begin_default_pass(&pass_action, d3d11_width(), d3d11_height());
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_draw(0, 6, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
