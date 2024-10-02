//------------------------------------------------------------------------------
//  offscreen-wgpu.c
//  Render to an offscreen rendertarget texture, and use this texture
//  for rendering to the display.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"

#define OFFSCREEN_SAMPLE_COUNT (1)
#define DISPLAY_SAMPLE_COUNT (4)
#define OFFSCREEN_COLOR_FORMAT SG_PIXELFORMAT_RGBA8
#define OFFSCREEN_DEPTH_FORMAT SG_PIXELFORMAT_DEPTH

static struct {
    struct {
        sg_pass_action pass_action;
        sg_attachments attachments;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } display;
    float rx, ry;
} state = {
    .offscreen.pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    },
    .display.pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.25f, 1.0f, 1.0f }
        }
    }
};

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = wgpu_environment(),
        .logger.func = slog_func,
    });

    // create one color and one depth render target image
    sg_image_desc img_desc = {
        .render_target = true,
        .width = 512,
        .height = 512,
        .pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = OFFSCREEN_DEPTH_FORMAT;
    sg_image depth_img = sg_make_image(&img_desc);

    // an offscreen render pass into those images
    state.offscreen.attachments = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = color_img,
        .depth_stencil.image = depth_img
    });
    assert(sg_wgpu_query_attachments_info(state.offscreen.attachments).color_view[0] != 0);
    assert(sg_wgpu_query_attachments_info(state.offscreen.attachments).color_view[1] == 0);
    assert(sg_wgpu_query_attachments_info(state.offscreen.attachments).resolve_view[0] == 0);
    assert(sg_wgpu_query_attachments_info(state.offscreen.attachments).ds_view != 0);

    // a sampler object for when the render target is used as texture
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    // cube vertex buffer with positions, colors and tex coords
    float vertices[] = {
        /* pos                  color                       uvs */
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
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
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
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // shader for non-textured cube, rendered in offscreen pass
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct vs_params {\n"
            "  mvp: mat4x4f,\n"
            "};\n"
            "@group(0) @binding(0) var<uniform> in: vs_params;\n"
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) color: vec4f,\n"
            "};\n"
            "@vertex fn main(@location(0) pos: vec4f, @location(1) color: vec4f) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = in.mvp * pos;\n"
            "  out.color = color;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "@fragment fn main(@location(0) color: vec4f) -> @location(0) vec4f {\n"
            "  return color;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .wgsl_group0_binding_n = 0,
        },
    });

    // and a second shader for rendering a textured cube in the default pass
    sg_shader display_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct vs_params {\n"
            "  mvp: mat4x4f,\n"
            "}\n"
            "@group(0) @binding(0) var<uniform> in: vs_params;\n"
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) color: vec4f,\n"
            "  @location(1) uv: vec2f,\n"
            "}\n"
            "@vertex fn main(@location(0) pos: vec4f, @location(1) color: vec4f, @location(2) uv: vec2f) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = in.mvp * pos;\n"
            "  out.color = color;\n"
            "  out.uv = uv;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "@group(1) @binding(0) var tex: texture_2d<f32>;\n"
            "@group(1) @binding(1) var smp: sampler;\n"
            "@fragment fn main(@location(0) color: vec4f, @location(1) uv: vec2f) -> @location(0) vec4f {\n"
            "  return textureSample(tex, smp, uv) + color * 0.5;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .wgsl_group0_binding_n = 0,
        },
        .images[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .wgsl_group1_binding_n = 0,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .wgsl_group1_binding_n = 1,
        },
        .image_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .image_slot = 0,
            .sampler_slot = 0,
        },
    });

    // pipeline-state-object for offscreen-rendered cube, don't need texture coord here
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // need to provide stride, because the buffer's texcoord is skipped
            .buffers[0].stride = 36,
            // but don't need to provide attr offsets, because pos and color are continuous
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .pixel_format = OFFSCREEN_DEPTH_FORMAT,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = OFFSCREEN_COLOR_FORMAT,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "offscreen-pipeline"
    });

    // and another pipeline-state-object for the default pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // don't need to provide buffer stride or attr offsets, no gaps here
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_FLOAT4,
                [2].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = display_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
        .label = "default-pipeline"
    });

    // the resource bindings for rendering a non-textured cube into offscreen render target
    state.offscreen.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    // resource bindings to render a textured cube, using the offscreen render target as texture
    state.display.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .images[0] = color_img,
        .samplers[0] = smp,
    };
}

static void frame(void) {
    // compute model-view-projection matrix for vertex shader, this will be
    // used both for the offscreen-pass, and the display-pass
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)wgpu_width()/(float)wgpu_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    const vs_params_t vs_params = {
        .mvp = HMM_MultiplyMat4(view_proj, model),
    };

    // the offscreen pass, rendering an rotating, untextured cube into a render target image
    sg_begin_pass(&(sg_pass){
        .action = state.offscreen.pass_action,
        .attachments = state.offscreen.attachments
    });
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // and the display-pass, rendering a rotating, textured cube, using the
    // previously rendered offscreen render-target as texture
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = wgpu_swapchain()
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .width = 640,
        .height = 480,
        .sample_count = DISPLAY_SAMPLE_COUNT,
        .title = "offscreen-wgpu"
    });
    return 0;
}
