//------------------------------------------------------------------------------
//  mipmap-wgpu.c
//  Demonstrate all the mipmapping filters.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "wgpu_entry.h"
#include "mipmap-wgpu.glsl.h"

#define SAMPLE_COUNT (4)

static struct {
    sg_pipeline pip;
    sg_buffer vbuf;
    sg_image img[12];
    float r;
    struct {
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
} state;

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

static void init(void) {
    sg_setup(&(sg_desc){
        .context = wgpu_get_context()
    });

    /* a plane vertex buffer */
    float vertices[] = {
        -1.0, -1.0, 0.0,  0.0, 0.0,
        +1.0, -1.0, 0.0,  1.0, 0.0,
        -1.0, +1.0, 0.0,  0.0, 1.0,
        +1.0, +1.0, 0.0,  1.0, 1.0,
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    });

    /* initialize mipmap content, different colors and checkboard pattern */
    sg_image_content img_content;
    uint32_t* ptr = state.pixels.mip0;
    bool even_odd = false;
    for (int mip_index = 0; mip_index <= 8; mip_index++) {
        const int dim = 1<<(8-mip_index);
        img_content.subimage[0][mip_index].ptr = ptr;
        img_content.subimage[0][mip_index].size = dim * dim * 4;
        for (int y = 0; y < dim; y++) {
            for (int x = 0; x < dim; x++) {
                *ptr++ = even_odd ? mip_colors[mip_index] : 0xFF000000;
                even_odd = !even_odd;
            }
            even_odd = !even_odd;
        }
    }
    /* the first 4 images are just different min-filters, the last
       4 images are different anistropy levels */
    sg_image_desc img_desc = {
        .width = 256,
        .height = 256,
        .num_mipmaps = 9,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .mag_filter = SG_FILTER_LINEAR,
        .content = img_content
    };
    sg_filter min_filter[] = {
        SG_FILTER_NEAREST_MIPMAP_NEAREST,
        SG_FILTER_LINEAR_MIPMAP_NEAREST,
        SG_FILTER_NEAREST_MIPMAP_LINEAR,
        SG_FILTER_LINEAR_MIPMAP_LINEAR,
    };
    for (int i = 0; i < 4; i++) {
        img_desc.min_filter = min_filter[i];
        state.img[i] = sg_make_image(&img_desc);
    }
    img_desc.min_lod = 2.0f;
    img_desc.max_lod = 4.0f;
    for (int i = 4; i < 8; i++) {
        img_desc.min_filter = min_filter[i-4];
        state.img[i] = sg_make_image(&img_desc);
    }
    img_desc.min_lod = 0.0f;
    img_desc.max_lod = 0.0f;    /* for max_lod, zero-initialized means "FLT_MAX" */
    for (int i = 8; i < 12; i++) {
        img_desc.max_anisotropy = 1<<(i-7);
        state.img[i] = sg_make_image(&img_desc);
    }

    /* pipeline state */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = {
            .attrs = {
                [ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_uv0].format = SG_VERTEXFORMAT_FLOAT2
            } 
        },
        .shader = sg_make_shader(mipmap_shader_desc()),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });
}

static void frame(void) {
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)wgpu_width()/(float)wgpu_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 5.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
    state.r += 0.1f;
    hmm_mat4 rm = HMM_Rotate(state.r, HMM_Vec3(1.0f, 0.0f, 0.0f));

    sg_bindings bind = {
        .vertex_buffers[0] = state.vbuf
    };
    sg_begin_default_pass(&(sg_pass_action){0}, wgpu_width(), wgpu_height());
    sg_apply_pipeline(state.pip);
    for (int i = 0; i < 12; i++) {
        const float x = ((float)(i & 3) - 1.5f) * 2.0f;
        const float y = ((float)(i / 4) - 1.0f) * -2.0f;
        hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
        
        bind.fs_images[SLOT_tex] = state.img[i];
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &vs_params, sizeof(vs_params));
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
