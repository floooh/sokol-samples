//------------------------------------------------------------------------------
//  blend-metal.c
//  Test blend factor combinations.
//------------------------------------------------------------------------------
#include <assert.h>
#include "osxentry.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

const int WIDTH = 800;
const int HEIGHT = 600;
const int MSAA_SAMPLES = 4;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

typedef struct {
    float tick;
} fs_params_t;

sg_draw_state draw_state;
#define NUM_BLEND_FACTORS (15)
sg_pipeline pips[NUM_BLEND_FACTORS][NUM_BLEND_FACTORS];
sg_pipeline bg_pip;
float r = 0.0f;
hmm_mat4 view_proj;
vs_params_t vs_params;
fs_params_t fs_params;

/* a pass action which does not clear, since the entire screen is overwritten anyway */
sg_pass_action pass_action = {
    .colors[0].action = SG_ACTION_DONTCARE ,
    .depth.action = SG_ACTION_DONTCARE,
    .stencil.action = SG_ACTION_DONTCARE
};

void init(const void* mtl_device) {
    /* setup sokol */
    sg_setup(&(sg_desc){
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1,
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    });

    /* a quad vertex buffer */
    float vertices[] = {
        /* pos               color */
        -1.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.5f,
        +1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.5f,
        -1.0f, +1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.5f,
        +1.0f, +1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.5f
    };
    draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* a shader for the fullscreen background quad */
    sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .entry = "vs_main",
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct vs_in {\n"
                "  float2 position[[attribute(0)]];\n"
                "};\n"
                "struct vs_out {\n"
                "  float4 pos [[position]];\n"
                "};\n"
                "vertex vs_out vs_main(vs_in in [[stage_in]]) {\n"
                "  vs_out out;\n"
                "  out.pos = float4(in.position, 0.5, 1.0);\n"
                "  return out;\n"
                "};\n"
        },
        .fs = {
            .uniform_blocks[0].size = sizeof(fs_params_t),
            .entry = "fs_main",
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct params_t {\n"
                "  float tick;\n"
                "};\n"
                "fragment float4 fs_main(float4 frag_coord [[position]], constant params_t& params [[buffer(0)]]) {\n"
                "  float2 xy = fract((frag_coord.xy-float2(params.tick)) / 50.0);\n"
                "  return float4(float3(xy.x*xy.y), 1.0);\n"
                "}\n"
        }
    });

    /* a pipeline state object for rendering the background quad */
    bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        /* we use the same vertex buffer as for the colored 3D quads,
           but only the first two floats from the position, need to
           provide a stride to skip the gap to the next vertex
        */
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT2 }
            }
        },
        .shader = bg_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .rasterizer.sample_count = MSAA_SAMPLES
    });

    /* a shader for the blended quads */
    sg_shader quad_shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .entry = "vs_main",
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct params_t {\n"
                "  float4x4 mvp;\n"
                "};\n"
                "struct vs_in {\n"
                "  float4 position [[attribute(0)]];\n"
                "  float4 color [[attribute(1)]];\n"
                "};\n"
                "struct vs_out {\n"
                "  float4 pos [[position]];\n"
                "  float4 color;\n"
                "};\n"
                "vertex vs_out vs_main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
                "  vs_out out;\n"
                "  out.pos = params.mvp * in.position;\n"
                "  out.color = in.color;\n"
                "  return out;\n"
                "}\n"
        },
        .fs = {
            .entry = "fs_main",
            .source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "struct fs_in {\n"
                "  float4 color;\n"
                "};\n"
                "fragment float4 fs_main(fs_in in [[stage_in]]) {\n"
                "  return in.color;\n"
                "};\n"
        }
    });

    /* one pipeline object per blend-factor combination */
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        },
        .shader = quad_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend = {
            .enabled = true,
            .blend_color = { 1.0f, 0.0f, 0.0f, 1.0f },
        },
        .rasterizer.sample_count = MSAA_SAMPLES
    };
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++) {
            pip_desc.blend.src_factor_rgb = (sg_blend_factor) (src + 1);
            pip_desc.blend.dst_factor_rgb = (sg_blend_factor) (dst + 1);
            pip_desc.blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
            pip_desc.blend.dst_factor_alpha = SG_BLENDFACTOR_ZERO;
            pips[src][dst] = sg_make_pipeline(&pip_desc);
            assert(pips[src][dst].id != SG_INVALID_ID);
        }
    }

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 100.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 25.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);
}

void frame() {
    sg_begin_default_pass(&(sg_pass_action){0}, osx_width(), osx_height());

    /* draw a background quad */
    draw_state.pipeline = bg_pip;
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_FS, 0, &fs_params, sizeof(fs_params));
    sg_draw(0, 4, 1);

    /* draw the blended quads */
    float r0 = r;
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
            /* compute new model-view-proj matrix */
            hmm_mat4 rm = HMM_Rotate(r0, HMM_Vec3(0.0f, 1.0f, 0.0f));
            const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
            const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
            hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
            vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

            draw_state.pipeline = pips[src][dst];
            sg_apply_draw_state(&draw_state);
            sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
            sg_draw(0, 4, 1);
        }
    }
    sg_end_pass();
    sg_commit();
    r += 0.6f;
    fs_params.tick += 1.0f;
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, MSAA_SAMPLES, "Sokol Blend (Metal)", init, frame, shutdown);
}

