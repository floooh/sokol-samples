//------------------------------------------------------------------------------
//  mipmap-wgpu.c
//  Demonstrate all the mipmapping filters.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"

#define SAMPLE_COUNT (4)

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static struct {
    uint32_t mip0[65536];   // 256x256
    uint32_t mip1[16384];   // 128x128
    uint32_t mip2[4096];    // 64*64
    uint32_t mip3[1024];    // 32*32
    uint32_t mip4[256];     // 16*16
    uint32_t mip5[64];      // 8*8
    uint32_t mip6[16];      // 4*4
    uint32_t mip7[4];       // 2*2
    uint32_t mip8[1];       // 1*2
} pixels;

static const uint32_t mip_colors[9] = {
    0xFF0000FF,     /* red */
    0xFF00FF00,     /* green */
    0xFFFF0000,     /* blue */
    0xFFFF00FF,     /* magenta */
    0xFFFFFF00,     /* cyan */
    0xFF00FFFF,     /* yellow */
    0xFFFF00A0,     /* violet */
    0xFFFFA0FF,     /* orange */
    0xFFA000FF,     /* purple */
};

static struct {
    sg_pipeline pip;
    sg_bindings bind;
    sg_sampler smp[12];
    float r;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = wgpu_environment(),
        .logger.func = slog_func,
    });

    // a plane vertex buffer
    float vertices[] = {
        -1.0, -1.0, 0.0,  0.0, 0.0,
        +1.0, -1.0, 0.0,  1.0, 0.0,
        -1.0, +1.0, 0.0,  0.0, 1.0,
        +1.0, +1.0, 0.0,  1.0, 1.0,
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // initialize mipmap content, different colors and checkboard pattern
    sg_image_data img_data;
    uint32_t* ptr = pixels.mip0;
    bool even_odd = false;
    for (int mip_index = 0; mip_index <= 8; mip_index++) {
        const int dim = 1<<(8-mip_index);
        img_data.subimage[0][mip_index].ptr = ptr;
        img_data.subimage[0][mip_index].size = (size_t) (dim * dim * 4);
        for (int y = 0; y < dim; y++) {
            for (int x = 0; x < dim; x++) {
                *ptr++ = even_odd ? mip_colors[mip_index] : 0xFF000000;
                even_odd = !even_odd;
            }
            even_odd = !even_odd;
        }
    }

    // create a single image and 12 samplers
    state.bind.fs.images[0] = sg_make_image(&(sg_image_desc){
        .width = 256,
        .height = 256,
        .num_mipmaps = 9,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data = img_data
    });

    // the first 4 samplers are just different min-filters
    sg_sampler_desc smp_desc = {
        .mag_filter = SG_FILTER_LINEAR,
    };
    sg_filter filters[] = {
        SG_FILTER_NEAREST,
        SG_FILTER_LINEAR,
    };
    sg_filter mipmap_filters[] = {
        SG_FILTER_NEAREST,
        SG_FILTER_LINEAR,
    };
    int smp_index = 0;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            smp_desc.min_filter = filters[i];
            smp_desc.mipmap_filter = mipmap_filters[j];
            state.smp[smp_index++] = sg_make_sampler(&smp_desc);
        }
    }
    // the next 4 samplers use min_lod/max_lod
    smp_desc.min_lod = 2.0f;
    smp_desc.max_lod = 4.0f;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            smp_desc.min_filter = filters[i];
            smp_desc.mipmap_filter = mipmap_filters[j];
            state.smp[smp_index++] = sg_make_sampler(&smp_desc);
        }
    }
    // the last 4 samplers use different anistropy levels
    smp_desc.min_lod = 0.0f;
    smp_desc.max_lod = 0.0f;    // for max_lod, zero-initialized means "FLT_MAX"
    smp_desc.min_filter = SG_FILTER_LINEAR;
    smp_desc.mag_filter = SG_FILTER_LINEAR;
    smp_desc.mipmap_filter = SG_FILTER_LINEAR;
    for (int i = 0; i < 4; i++) {
        smp_desc.max_anisotropy = 1<<i;
        state.smp[smp_index++] = sg_make_sampler(&smp_desc);
    }
    assert(smp_index == 12);

    // a shader object
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .source =
                "struct vs_params {\n"
                "  mvp: mat4x4f,\n"
                "}\n"
                "@group(0) @binding(0) var<uniform> in: vs_params;\n"
                "struct vs_out {\n"
                "  @builtin(position) pos: vec4f,\n"
                "  @location(0) uv: vec2f,\n"
                "}\n"
                "@vertex fn main(@location(0) pos: vec4f, @location(1) uv: vec2f) -> vs_out {\n"
                "  var out: vs_out;\n"
                "  out.pos = in.mvp * pos;\n"
                "  out.uv = uv;\n"
                "  return out;\n"
                "}\n"
        },
        .fs = {
            .images[0].used = true,
            .samplers[0].used = true,
            .image_sampler_pairs[0] = { .used = true, .image_slot = 0, .sampler_slot = 0 },
            .source =
                "@group(1) @binding(32) var tex: texture_2d<f32>;\n"
                "@group(1) @binding(48) var smp: sampler;\n"
                "@fragment fn main(@location(0) uv: vec2f) -> @location(0) vec4f {\n"
                "  return textureSample(tex, smp, uv);\n"
                "}\n"
        }
    });

    // pipeline state
    state.pip = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });
}

static void frame(void) {
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)wgpu_width()/(float)wgpu_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 5.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.r += 0.1f;
    hmm_mat4 rm = HMM_Rotate(state.r, HMM_Vec3(1.0f, 0.0f, 0.0f));

    sg_begin_pass(&(sg_pass){ .swapchain = wgpu_swapchain() });
    sg_apply_pipeline(state.pip);
    for (int i = 0; i < 12; i++) {
        const float x = ((float)(i & 3) - 1.5f) * 2.0f;
        const float y = ((float)(i / 4) - 1.0f) * -2.0f;
        hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
        state.bind.fs.samplers[0] = state.smp[i];
        sg_apply_bindings(&state.bind);
        const vs_params_t vs_params = { .mvp = HMM_MultiplyMat4(view_proj, model) };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
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
        .title = "mipmap-wgpu"
    });
    return 0;
}
