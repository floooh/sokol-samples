//------------------------------------------------------------------------------
//  mipmap-d3d11.c
//  Test mipmapping behaviour.
//  Top row: NEAREST_MIPMAP_NEAREST to LINEAR_MIPMAP_LINEAR
//  Middle row: mipmap min/max lod test
//  Bottom row: anistropy levels 2, 4, 8 and 16
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

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

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const int SAMPLE_COUNT = 4;
    d3d11_init(WIDTH, HEIGHT, SAMPLE_COUNT, L"Sokol Mipmap D3D11");
    sg_setup(&(sg_desc){
        .context = d3d11_get_context()
    });

    /* a plane vertex buffer */
    float vertices[] = {
        -1.0, -1.0, 0.0,  0.0, 0.0,
        +1.0, -1.0, 0.0,  1.0, 0.0,
        -1.0, +1.0, 0.0,  0.0, 1.0,
        +1.0, +1.0, 0.0,  1.0, 1.0,
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    /* initialize mipmap content, different colors and checkboard pattern */
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
    /* the first 4 images are just different min-filters, the last
       4 images are different anistropy levels */
    sg_image img[12];
    sg_image_desc img_desc = {
        .width = 256,
        .height = 256,
        .num_mipmaps = 9,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .mag_filter = SG_FILTER_LINEAR,
        .data = img_data
    };
    sg_filter min_filter[] = {
        SG_FILTER_NEAREST_MIPMAP_NEAREST,
        SG_FILTER_LINEAR_MIPMAP_NEAREST,
        SG_FILTER_NEAREST_MIPMAP_LINEAR,
        SG_FILTER_LINEAR_MIPMAP_LINEAR,
    };
    for (int i = 0; i < 4; i++) {
        img_desc.min_filter = min_filter[i];
        img[i] = sg_make_image(&img_desc);
    }
    img_desc.min_lod = 2.0f;
    img_desc.max_lod = 4.0f;
    for (int i = 4; i < 8; i++) {
        img_desc.min_filter = min_filter[i-4];
        img[i] = sg_make_image(&img_desc);
    }
    img_desc.min_lod = 0.0f;
    img_desc.max_lod = 0.0f;    /* for max_lod, zero-initialized means "FLT_MAX" */
    for (int i = 8; i < 12; i++) {
        img_desc.max_anisotropy = 1<<(i-7);
        img[i] = sg_make_image(&img_desc);
    }

    /* shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].sem_name = "POSITION",
            [1].sem_name = "TEXCOORD"
        },
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .source =
                "cbuffer params: register(b0) {\n"
                "  float4x4 mvp;\n"
                "};\n"
                "struct vs_in {\n"
                "  float4 pos: POSITION;\n"
                "  float2 uv: TEXCOORD0;\n"
                "};\n"
                "struct vs_out {\n"
                "  float2 uv: TEXCOORD0;\n"
                "  float4 pos: SV_Position;\n"
                "};\n"
                "vs_out main(vs_in inp) {\n"
                "  vs_out outp;\n"
                "  outp.pos = mul(mvp, inp.pos);\n"
                "  outp.uv = inp.uv;\n"
                "  return outp;\n"
                "};\n"
        },
        .fs = {
            .images[0].image_type = SG_IMAGETYPE_2D,
            .source =
                "Texture2D<float4> tex: register(t0);\n"
                "sampler smp: register(s0);\n"
                "float4 main(float2 uv: TEXCOORD0): SV_Target0 {\n"
                "  return tex.Sample(smp, uv);\n"
                "};\n"
        }
    });

    /* pipeline state */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 5.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    vs_params_t vs_params;
    float r = 0.0f;
    while (d3d11_process_events()) {
        r += 0.1f;
        hmm_mat4 rm = HMM_Rotate(r, HMM_Vec3(1.0f, 0.0f, 0.0f));

        sg_begin_default_pass(&(sg_pass_action){0}, d3d11_width(), d3d11_height());
        sg_apply_pipeline(pip);
        sg_bindings bind = {
            .vertex_buffers[0] = vbuf
        };
        for (int i = 0; i < 12; i++) {
            const float x = ((float)(i & 3) - 1.5f) * 2.0f;
            const float y = ((float)(i / 4) - 1.0f) * -2.0f;
            hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
            vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

            bind.fs_images[0] = img[i];
            sg_apply_bindings(&bind);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
            sg_draw(0, 4, 1);
        }
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
}
