//------------------------------------------------------------------------------
//  blend-sapp.c
//  Test/demonstrate blend modes.
//------------------------------------------------------------------------------
#include <assert.h>
#include "sokol_app.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

const char *bg_vs_src, *bg_fs_src, *quad_vs_src, *quad_fs_src;

#define MSAA_SAMPLES (4)

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

typedef struct {
    float tick;
} fs_params_t;

static sg_bindings bind;
#define NUM_BLEND_FACTORS (15)
static sg_pipeline pips[NUM_BLEND_FACTORS][NUM_BLEND_FACTORS];
static sg_pipeline bg_pip;
static float r;
static vs_params_t vs_params;
static fs_params_t fs_params;

/* a pass action which does not clear, since the entire screen is overwritten anyway */
static sg_pass_action pass_action = {
    .colors[0].action = SG_ACTION_DONTCARE ,
    .depth.action = SG_ACTION_DONTCARE,
    .stencil.action = SG_ACTION_DONTCARE
};

void init(void) {
    sg_setup(&(sg_desc){
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1,
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });

    /* a quad vertex buffer */
    float vertices[] = {
        /* pos               color */
        -1.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.5f,
        +1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.5f,
        -1.0f, +1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.5f,
        +1.0f, +1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.5f
    };
    bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* a shader for the fullscreen background quad */
    sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
        .vs.source = bg_vs_src,
        .fs = {
            .uniform_blocks[0] = {
                .size = sizeof(fs_params_t),
                .uniforms = {
                    [0] = { .name="tick", .type=SG_UNIFORMTYPE_FLOAT }
                }
            },
            .source = bg_fs_src
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
                [0] = { .name="position", .sem_name="POS", .format=SG_VERTEXFORMAT_FLOAT2 }
            }
        },
        .shader = bg_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .rasterizer.sample_count = MSAA_SAMPLES
    });

    /* a shader for the blended quads */
    sg_shader quad_shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0] = {
                .size = sizeof(vs_params_t),
                .uniforms = {
                    [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
                }
            },
            .source = quad_vs_src,
        },
        .fs.source = quad_fs_src,
    });

    /* one pipeline object per blend-factor combination */
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0] = { .name="position", .sem_name="POS", .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="color0", .sem_name="COLOR", .format=SG_VERTEXFORMAT_FLOAT4 }
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
            const sg_blend_factor src_blend = (sg_blend_factor) (src+1);
            const sg_blend_factor dst_blend = (sg_blend_factor) (dst+1);
            /* WebGL exceptions: 
                - "GL_SRC_ALPHA_SATURATE as a destination blend function is disallowed in WebGL 1"
                - "constant color and constant alpha cannot be used together as source and 
                   destination factors in the blend function"
            */
            bool valid = true;
            if (dst_blend == SG_BLENDFACTOR_SRC_ALPHA_SATURATED) {
                valid = false;
            }
            else if ((src_blend == SG_BLENDFACTOR_BLEND_COLOR) || (src_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_COLOR)) {
                if ((dst_blend == SG_BLENDFACTOR_BLEND_ALPHA) || (dst_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_ALPHA)) {
                    valid = false;
                }
            }
            else if ((src_blend == SG_BLENDFACTOR_BLEND_ALPHA) || (src_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_ALPHA)) {
                if ((dst_blend == SG_BLENDFACTOR_BLEND_COLOR) || (dst_blend == SG_BLENDFACTOR_ONE_MINUS_BLEND_COLOR)) {
                    valid = false;
                }
            }
            if (valid) {
                pip_desc.blend.src_factor_rgb = src_blend;
                pip_desc.blend.dst_factor_rgb = dst_blend;
                pip_desc.blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
                pip_desc.blend.dst_factor_alpha = SG_BLENDFACTOR_ZERO;
                pips[src][dst] = sg_make_pipeline(&pip_desc);
                assert(pips[src][dst].id != SG_INVALID_ID);
            }
            else {
                pips[src][dst].id = SG_INVALID_ID;
            }
        }
    }
}

void frame(void) {
    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 100.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 25.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* start rendering */
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());

    /* draw a background quad */
    sg_apply_pipeline(bg_pip);
    sg_apply_bindings(&bind);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_params, sizeof(fs_params));
    sg_draw(0, 4, 1);

    /* draw the blended quads */
    float r0 = r;
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
            if (pips[src][dst].id != SG_INVALID_ID) {
                /* compute new model-view-proj matrix */
                hmm_mat4 rm = HMM_Rotate(r0, HMM_Vec3(0.0f, 1.0f, 0.0f));
                const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
                const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
                hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
                vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

                sg_apply_pipeline(pips[src][dst]);
                sg_apply_bindings(&bind);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
                sg_draw(0, 4, 1);
            }
        }
    }
    sg_end_pass();
    sg_commit();
    r += 0.6f;
    fs_params.tick += 1.0f;
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
        .sample_count = MSAA_SAMPLES,
        .gl_force_gles2 = true,
        .window_title = "Blend Modes (sokol-app)",
    };
}

#if defined(SOKOL_GLCORE33)
const char* bg_vs_src =
    "#version 330\n"
    "in vec2 position;\n"
    "void main() {\n"
    "  gl_Position = vec4(position, 0.5, 1.0);\n"
    "}\n";
const char* bg_fs_src =
    "#version 330\n"
    "uniform float tick;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);\n"
    "  frag_color = vec4(vec3(xy.x*xy.y), 1.0);\n"
    "}\n";
const char* quad_vs_src =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec4 position;\n"
    "in vec4 color0;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  color = color0;\n"
    "}\n";
const char* quad_fs_src =
    "#version 330\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = color;\n"
    "}";
#elif defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
const char* bg_vs_src =
    "attribute vec2 position;\n"
    "void main() {\n"
    "  gl_Position = vec4(position, 0.5, 1.0);\n"
    "}\n";
const char* bg_fs_src =
    "precision mediump float;\n"
    "uniform float tick;\n"
    "void main() {\n"
    "  vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);\n"
    "  gl_FragColor = vec4(vec3(xy.x*xy.y), 1.0);\n"
    "}\n";
const char* quad_vs_src =
    "uniform mat4 mvp;\n"
    "attribute vec4 position;\n"
    "attribute vec4 color0;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;"
    "  color = color0;\n"
    "}\n";
const char* quad_fs_src =
    "precision mediump float;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "  gl_FragColor = color;\n"
    "}";
#elif defined(SOKOL_METAL)
const char* bg_vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct vs_in {\n"
    "  float2 position[[attribute(0)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]]) {\n"
    "  vs_out out;\n"
    "  out.pos = float4(in.position, 0.5, 1.0);\n"
    "  return out;\n"
    "};\n";
const char* bg_fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float tick;\n"
    "};\n"
    "fragment float4 _main(float4 frag_coord [[position]], constant params_t& params [[buffer(0)]]) {\n"
    "  float2 xy = fract((frag_coord.xy-float2(params.tick)) / 50.0);\n"
    "  return float4(float3(xy.x*xy.y), 1.0);\n"
    "}\n";
const char* quad_vs_src =
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
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.position;\n"
    "  out.color = in.color;\n"
    "  return out;\n"
    "}\n";
const char* quad_fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float4 color;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]]) {\n"
    "  return in.color;\n"
    "};\n";
#elif defined(SOKOL_D3D11)
const char* bg_vs_src =
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
    "};\n";
const char* bg_fs_src =
    "cbuffer params: register(b0) {\n"
    "  float tick;\n"
    "};\n"
    "float4 main(float4 frag_coord: SV_Position): SV_Target0 {\n"
    "  float2 xy = frac((frag_coord.xy-float2(tick,tick)) / 50.0);\n"
    "  float c = xy.x * xy.y;\n"
    "  return float4(c, c, c, 1.0);\n"
    "};\n";
const char* quad_vs_src =
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
    "}\n";
const char* quad_fs_src =
    "float4 main(float4 color: COLOR): SV_Target0 {\n"
    "  return color;\n"
    "}\n";
#endif
