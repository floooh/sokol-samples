//------------------------------------------------------------------------------
//  mipmap-emsc.c
//  Test mipmapping behaviour.
//  Top row: NEAREST_MIPMAP_NEAREST to LINEAR_MIPMAP_LINEAR
//  Bottom row: anistropy levels 2, 4, 8 and 16
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static struct {
    uint32_t mip0[65536];   /* 256x256 */
    uint32_t mip1[16384];   /* 128x128 */
    uint32_t mip2[4096];    /* 64*64 */
    uint32_t mip3[1024];    /* 32*32 */
    uint32_t mip4[256];     /* 16*16 */
    uint32_t mip5[64];      /* 8*8 */
    uint32_t mip6[16];      /* 4*4 */
    uint32_t mip7[4];       /* 2*2 */
    uint32_t mip8[1];       /* 1*2 */
} pixels;

static uint32_t mip_colors[9] = {
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

static EM_BOOL draw(double time, void* userdata);

int main() {
    // try to setup WebGL2 context (for the mipmap min/max lod stuff)
    emsc_init("#canvas", EMSC_ANTIALIAS);

    // setup sokol_gfx
    sg_desc desc = {
        .logger.func = slog_func,
    };
    sg_setup(&desc);

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
    smp_desc.min_filter = SG_FILTER_NEAREST;
    smp_desc.mag_filter = SG_FILTER_NEAREST;
    smp_desc.mipmap_filter = SG_FILTER_LINEAR;
    for (int i = 0; i < 4; i++) {
        smp_desc.max_anisotropy = 1<<i;
        state.smp[smp_index++] = sg_make_sampler(&smp_desc);
    }
    assert(smp_index == 12);

    // shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].name = "position",
            [1].name = "texcoord0"
        },
        .vs = {
            .uniform_blocks[0] = {
                .size = sizeof(vs_params_t),
                .uniforms = {
                    [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
                }
            },
            .source =
                "uniform mat4 mvp;\n"
                "attribute vec4 position;\n"
                "attribute vec2 texcoord0;\n"
                "varying vec2 uv;\n"
                "void main() {\n"
                "  gl_Position = mvp * position;\n"
                "  uv = texcoord0;\n"
                "}\n"
        },
        .fs = {
            .images[0].used = true,
            .samplers[0].used = true,
            .image_sampler_pairs[0] = { .used = true, .glsl_name = "tex", .image_slot = 0, .sampler_slot = 0 },
            .source =
                "precision mediump float;"
                "uniform sampler2D tex;"
                "varying vec2 uv;\n"
                "void main() {\n"
                "  gl_FragColor = texture2D(tex, uv);\n"
                "}\n"
        }
    });

    // pipeline state
    state.pip = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    // hand off control to browser loop
    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    state.r += 0.1f;

    // view-projection matrix
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)emsc_width()/(float)emsc_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 5.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    vs_params_t vs_params;
    hmm_mat4 rm = HMM_Rotate(state.r, HMM_Vec3(1.0f, 0.0f, 0.0f));

    sg_begin_default_pass(&(sg_pass_action){0}, emsc_width(), emsc_height());
    sg_apply_pipeline(state.pip);
    for (int i = 0; i < 12; i++) {
        const float x = ((float)(i & 3) - 1.5f) * 2.0f;
        const float y = ((float)(i / 4) - 1.0f) * -2.0f;
        hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        state.bind.fs.samplers[0] = state.smp[i];
        sg_apply_bindings(&state.bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}
