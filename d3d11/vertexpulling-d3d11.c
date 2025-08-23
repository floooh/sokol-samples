//------------------------------------------------------------------------------
//  vertexpulling-d3d11.c
//
//  Demonstrates vertex pulling from storage buffers.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_gfx.h"
#include "sokol_log.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

typedef struct {
    float pos[4];
    float color[4];
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    d3d11_init(&(d3d11_desc_t){ .width = 640, .height = 480, .sample_count = 4, .title = L"vertexpulling-d3d11.c" });
    sg_setup(&(sg_desc){
        .environment = d3d11_environment(),
        .logger.func = slog_func,
    });

    const sg_pass_action pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f } },
    };

    const vertex_t vertices[] = {
        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },

        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },

        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },

        { .pos = { 1.0, -1.0, -1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0,  1.0, -1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0,  1.0,  1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0, -1.0,  1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },

        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },

        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } }
    };
    sg_buffer sbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .data = SG_RANGE(vertices),
    });
    sg_view sbuf_view = sg_make_view(&(sg_view_desc){ .storage_buffer.buffer = sbuf });

    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "cbuffer params: register(b0) {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "ByteAddressBuffer vertices: register(t0);\n"
            "struct vs_out {\n"
            "  float4 color: COLOR0;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(uint vidx: SV_VertexID) {\n"
            "  vs_out outp;\n"
            "  float4 pos = asfloat(vertices.Load4(vidx * 32 + 0));\n"
            "  float4 color = asfloat(vertices.Load4(vidx * 32 + 16));\n"
            "  outp.pos = mul(mvp, pos);\n"
            "  outp.color = color;\n"
            "  return outp;\n"
            "};\n",
        .fragment_func.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .hlsl_register_b_n = 0,
        },
        .views[0].storage_buffer = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .readonly = true,
            .hlsl_register_t_n = 0,
        },
    });

    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    const sg_bindings bind = {
        .index_buffer = ibuf,
        .views[0] = sbuf_view,
    };

    float rx = 0.0f, ry = 0.0f;
    while (d3d11_process_events()) {
        // view-projection matrix
        const hmm_mat4 proj = HMM_Perspective(60.0f, (float)d3d11_width()/(float)d3d11_height(), 0.01f, 10.0f);
        const hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        const hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
        // model-view-proj matrix for vertex shader
        vs_params_t vs_params;
        rx += 1.0f; ry += 2.0f;
        const hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        const hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        const hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = d3d11_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
