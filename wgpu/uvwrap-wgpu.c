//------------------------------------------------------------------------------
//  uvwrap-wgpu.c
//  Demonstrates and tests texture coordinate wrapping modes.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"

#define SAMPLE_COUNT (4)

static struct {
    sg_buffer vbuf;
    sg_image img;
    sg_sampler smp[_SG_WRAP_NUM];
    sg_pipeline pip;
    sg_pass_action pass_action;
} state = {
    .pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.5f, 0.7f, 1.0f } }
    }
};

typedef struct {
    float offset[2];
    float scale[2];
} vs_params_t;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = wgpu_environment(),
        .logger.func = slog_func,
    });

    // a quad vertex buffer with "oversized" texture coords
    const float quad_vertices[] = {
        -1.0f, +1.0f,
        +1.0f, +1.0f,
        -1.0f, -1.0f,
        +1.0f, -1.0f,
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices)
    });

    // an image object with a test pattern
    const uint32_t o = 0xFF555555;
    const uint32_t W = 0xFFFFFFFF;
    const uint32_t R = 0xFF0000FF;
    const uint32_t G = 0xFF00FF00;
    const uint32_t B = 0xFFFF0000;
    const uint32_t test_pixels[8][8] = {
        { R, R, R, R, G, G, G, G },
        { R, o, o, o, o, o, o, G },
        { R, o, o, o, o, o, o, G },
        { R, o, o, W, W, o, o, G },
        { B, o, o, W, W, o, o, R },
        { B, o, o, o, o, o, o, R },
        { B, o, o, o, o, o, o, R },
        { B, B, B, B, R, R, R, R },
    };
    state.img = sg_make_image(&(sg_image_desc){
        .width = 8,
        .height = 8,
        .data.subimage[0][0] = SG_RANGE(test_pixels),
        .label = "uvwrap-img",
    });

    // one sampler object per uv-wrap mode
    for (int i = SG_WRAP_REPEAT; i <= SG_WRAP_MIRRORED_REPEAT; i++) {
        state.smp[i] = sg_make_sampler(&(sg_sampler_desc){
            .wrap_u = (sg_wrap) i,
            .wrap_v = (sg_wrap) i,
            .border_color = SG_BORDERCOLOR_OPAQUE_BLACK,
            .label = "uvwrap-sampler",
        });
    }

    // a shader object
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct vs_params {\n"
            "  offset: vec2f,\n"
            "  scale: vec2f,\n"
            "}\n"
            "@group(0) @binding(0) var<uniform> in: vs_params;\n"
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) uv: vec2f,\n"
            "}\n"
            "@vertex fn main(@location(0) pos: vec2f) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = vec4(pos * in.scale + in.offset, 0.5, 1.0);\n"
            "  out.uv = (pos + 1.0) - 0.5;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "@group(1) @binding(0) var tex: texture_2d<f32>;\n"
            "@group(1) @binding(1) var smp: sampler;\n"
            "@fragment fn main(@location(0) uv: vec2f) -> @location(0) vec4f {\n"
            "  return textureSample(tex, smp, uv);\n"
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
        .label = "uvwrap-shader",
    });

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .label = "uvwrap-pipeline",
    });
}

static void frame(void) {
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = wgpu_swapchain() });
    sg_apply_pipeline(state.pip);
    for (int i = SG_WRAP_REPEAT; i <= SG_WRAP_MIRRORED_REPEAT; i++) {
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.vbuf,
            .images[0] = state.img,
            .samplers[0] = state.smp[i],
        });
        float x_offset = 0, y_offset = 0;
        switch (i) {
            case SG_WRAP_REPEAT:            x_offset = -0.5f; y_offset = 0.5f; break;
            case SG_WRAP_CLAMP_TO_EDGE:     x_offset = +0.5f; y_offset = 0.5f; break;
            case SG_WRAP_CLAMP_TO_BORDER:   x_offset = -0.5f; y_offset = -0.5f; break;
            case SG_WRAP_MIRRORED_REPEAT:   x_offset = +0.5f; y_offset = -0.5f; break;
        }
        vs_params_t vs_params = {
            .offset = { x_offset, y_offset },
            .scale = { 0.4f, 0.4f }
        };
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 4, 1);
    }
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
        .sample_count = SAMPLE_COUNT,
        .width = 640,
        .height = 480,
        .title = "uvwrap-wgpu"
    });
    return 0;
}
