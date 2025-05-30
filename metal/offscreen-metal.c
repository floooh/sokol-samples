//------------------------------------------------------------------------------
//  offscreen-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

#define WIDTH (640)
#define HEIGHT (480)
#define SAMPLE_COUNT (4)

static struct {
    sg_attachments offscreen_attachments;
    sg_pipeline offscreen_pip;
    sg_bindings offscreen_bind;
    sg_pipeline default_pip;
    sg_bindings default_bind;
    sg_pass_action offscreen_pass_action;
    sg_pass_action default_pass_action;
    float rx, ry;
    hmm_mat4 view_proj;
} state = {
    // offscreen: clear to black, don't store MSAA surface content
    .offscreen_pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .store_action = SG_STOREACTION_DONTCARE,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    },
    // display: clear to blue-ish
    .default_pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.25f, 1.0f, 1.0f }
        }
    }
};

// vertex-shader params (just a model-view-projection matrix)
typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static void init(void) {
    // setup sokol
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    // a render pass with one color-, one resolve- and one depth-attachment image
    sg_image color_img = sg_make_image(&(sg_image_desc){
        .usage.render_attachment = true,
        .width = 256,
        .height = 256,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = SAMPLE_COUNT,
    });
    sg_image resolve_img = sg_make_image(&(sg_image_desc){
        .usage.render_attachment = true,
        .width = 256,
        .height = 256,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
    });
    sg_sampler resolve_smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });
    sg_image depth_img = sg_make_image(&(sg_image_desc){
        .usage.render_attachment = true,
        .width = 256,
        .height = 256,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = SAMPLE_COUNT,
    });
    state.offscreen_attachments = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = color_img,
        .resolves[0].image = resolve_img,
        .depth_stencil.image = depth_img
    });

    // cube vertex buffer with positions, colors and tex coords
    float vertices[] = {
        // pos                  color                       uvs
        -1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,

         1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,

        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 1.0f
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // an index buffer for the cube
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

    // the offscreen resource bindings for rendering a non-textured cube into render target
    state.offscreen_bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    // and the bindings to render a textured cube, using the msaa-resolve image as texture
    state.default_bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .images[0] = resolve_img,
        .samplers[0] = resolve_smp,
    };

    // a shader for a non-textured cube, rendered in the offscreen pass
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "};\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
    });

    // ...and another shader for the display-pass, rendering a textured cube
    // using the offscreen render target as texture */
    sg_shader default_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 position [[attribute(0)]];\n"
            "  float4 color [[attribute(1)]];\n"
            "  float2 uv [[attribute(2)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float4 color;\n"
            "  float2 uv;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  out.pos = params.mvp * in.position;\n"
            "  out.color = in.color;\n"
            "  out.uv = in.uv;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct fs_in {\n"
            "  float4 color;\n"
            "  float2 uv;\n"
            "};\n"
            "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
            "  return float4(tex.sample(smp, in.uv).xyz + in.color.xyz * 0.5, 1.0);\n"
            "};\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
        .images[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .msl_texture_n = 0,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .msl_sampler_n = 0,
        },
        .image_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .image_slot = 0,
            .sampler_slot = 0,
        },
    });

    // pipeline-state-object for offscreen-rendered cube, don't need texture coord here
    state.offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* need to set stride to skip uv-coords gap */
            .buffers[0].stride = 36,
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 },   /* position */
                [1] = { .format = SG_VERTEXFORMAT_FLOAT4 },   /* color */
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = SAMPLE_COUNT,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8
    });

    // and another pipeline-state-object for the default pass
    state.default_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 },    /* position */
                [1] = { .format = SG_VERTEXFORMAT_FLOAT4 },   /* color */
                [2] = { .format = SG_VERTEXFORMAT_FLOAT2 },   /* uv */
            }
        },
        .shader = default_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .sample_count = SAMPLE_COUNT,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    // view-projection matrix
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    state.view_proj = HMM_MultiplyMat4(proj, view);
}

static void frame(void) {
    // compute model-view-projection matrix for vertex shader, this will be
    // used both for the offscreen-pass, and the display-pass
    vs_params_t vs_params;
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(state.view_proj, model);

    // the offscreen pass, rendering an rotating, untextured cube into a render target image
    sg_begin_pass(&(sg_pass){
        .action = state.offscreen_pass_action,
        .attachments = state.offscreen_attachments,
    });
    sg_apply_pipeline(state.offscreen_pip);
    sg_apply_bindings(&state.offscreen_bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // and the display-pass, rendering a rotating, textured cube, using the
    // previously rendered offscreen render-target as texture
    sg_begin_pass(&(sg_pass){
        .action = state.default_pass_action,
        .swapchain = osx_swapchain()
    });
    sg_apply_pipeline(state.default_pip);
    sg_apply_bindings(&state.default_bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, SAMPLE_COUNT, SG_PIXELFORMAT_DEPTH_STENCIL, "offscreen-metal", init, frame, shutdown);
    return 0;
}
