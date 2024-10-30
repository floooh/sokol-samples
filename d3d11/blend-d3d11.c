//------------------------------------------------------------------------------
//  blend-d3d11.c
//  Test blend factor combinations.
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
    hmm_mat4 mvp;
} vs_params_t;

typedef struct {
    float tick;
} fs_params_t;

enum { NUM_BLEND_FACTORS = 15 };
static sg_pipeline pips[NUM_BLEND_FACTORS][NUM_BLEND_FACTORS];

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d11 app wrapper and sokol_gfx
    const int WIDTH = 800;
    const int HEIGHT = 600;
    d3d11_init(&(d3d11_desc_t){ .width = WIDTH, .height = HEIGHT, .sample_count = 4, .title = L"blend-d3d11.c" });
    sg_setup(&(sg_desc){
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1,
        .environment = d3d11_environment(),
        .logger.func = slog_func,
    });

    // a quad vertex buffer
    float vertices[] = {
        // pos               color
        -1.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.5f,
        +1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.5f,
        -1.0f, +1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.5f,
        +1.0f, +1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.5f
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // a shader for the fullscreen background quad
    sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct vs_in {\n"
            "  float2 pos: POS;\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = float4(inp.pos, 0.5, 1.0);\n"
            "  return outp;\n"
            "};\n",
        .fragment_func.source =
            "cbuffer params: register(b0) {\n"
            "  float tick;\n"
            "};\n"
            "float4 main(float4 frag_coord: SV_Position): SV_Target0 {\n"
            "  float2 xy = frac((frag_coord.xy-float2(tick,tick)) / 50.0);\n"
            "  float c = xy.x * xy.y;\n"
            "  return float4(c, c, c, 1.0);\n"
            "};\n",
        .attrs[0].hlsl_sem_name = "POS",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(fs_params_t),
            .hlsl_register_b_n = 0,
        },
    });

    // a pipeline state object for rendering the background quad
    sg_pipeline bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = 28,
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .shader = bg_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    // a shader for the blended quads
    sg_shader quad_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "cbuffer params: register(b0) {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 pos: POS;\n"
            "  float4 color: COLOR;\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 color: COLOR;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = mul(mvp, inp.pos);\n"
            "  outp.color = inp.color;\n"
            "  return outp;\n"
            "}\n",
        .fragment_func.source =
            "float4 main(float4 color: COLOR): SV_Target0 {\n"
            "  return color;\n"
            "}\n",
        .attrs = {
            [0].hlsl_sem_name = "POS",
            [1].hlsl_sem_name = "COLOR"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .hlsl_register_b_n = 0,
        },
    });

    // one pipeline object per blend-factor combination
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = quad_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend_color = { 1.0f, 0.0f, 0.0f, 1.0f },
    };
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++) {
            pip_desc.colors[0].blend = (sg_blend_state) {
                .enabled = true,
                .src_factor_rgb = (sg_blend_factor) (src + 1),
                .dst_factor_rgb = (sg_blend_factor) (dst + 1),
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ZERO,
            };
            pips[src][dst] = sg_make_pipeline(&pip_desc);
            assert(pips[src][dst].id != SG_INVALID_ID);
        }
    }

    // a pass action which does not clear, since the entire screen is overwritten anyway
    sg_pass_action pass_action = {
        .colors[0].load_action = SG_LOADACTION_DONTCARE ,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    // view-projection matrix
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 100.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 25.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // resource bindings
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf
    };

    vs_params_t vs_params;
    fs_params_t fs_params;
    float r = 0.0f;
    fs_params.tick = 0.0f;
    while (d3d11_process_events()) {
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = d3d11_swapchain() });

        // draw a background quad
        sg_apply_pipeline(bg_pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(fs_params));
        sg_draw(0, 4, 1);

        // draw the blended quads
        float r0 = r;
        for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
            for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
                // compute new model-view-proj matrix
                hmm_mat4 rm = HMM_Rotate(r0, HMM_Vec3(0.0f, 1.0f, 0.0f));
                const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
                const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
                hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
                vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

                sg_apply_pipeline(pips[src][dst]);
                sg_apply_bindings(&bind);
                sg_apply_uniforms(0, &SG_RANGE(vs_params));
                sg_draw(0, 4, 1);
            }
        }
        sg_end_pass();
        sg_commit();
        d3d11_present();
        r += 0.6f;
        fs_params.tick += 1.0f;
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
