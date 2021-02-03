//------------------------------------------------------------------------------
//  inject-d3d11.c
//  Demonstrate usage of injected native D3D11 buffer and image resources.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

// constants (VS doesn't like "const int" for array size)
enum {
    WIDTH = 640,
    HEIGHT = 480,
    IMG_WIDTH = 32,
    IMG_HEIGHT = 32,
};

uint32_t pixels[IMG_WIDTH * IMG_HEIGHT];

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    /* setup d3d11 app wrapper and sokol_gfx */
    const int sample_count = 4;
    const int width = 640;
    const int height = 480;
    d3d11_init(width, height, sample_count, L"Sokol Injected Resources D3D11");
    sg_setup(&(sg_desc){
        .context = d3d11_get_context()
    });

    /* create native D3D11 vertex and index buffers */
    float vertices[] = {
        /* pos                  uvs */
        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

         1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 1.0f
    };
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    ID3D11Device* d3d11_dev = (ID3D11Device*) d3d11_get_context().d3d11.device;
    D3D11_BUFFER_DESC d3d11_vbuf_desc = {
        .ByteWidth = sizeof(vertices),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER
    };
    D3D11_SUBRESOURCE_DATA d3d11_vbuf_data = {
        .pSysMem = vertices
    };
    ID3D11Buffer* d3d11_vbuf = 0;
    HRESULT hr = d3d11_dev->lpVtbl->CreateBuffer(d3d11_dev, &d3d11_vbuf_desc, &d3d11_vbuf_data, &d3d11_vbuf);
    assert(SUCCEEDED(hr) && d3d11_vbuf);
    D3D11_BUFFER_DESC d3d11_ibuf_desc = {
        .ByteWidth = sizeof(vertices),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_INDEX_BUFFER
    };
    D3D11_SUBRESOURCE_DATA d3d11_ibuf_data = {
        .pSysMem = indices
    };
    ID3D11Buffer* d3d11_ibuf = 0;
    hr = d3d11_dev->lpVtbl->CreateBuffer(d3d11_dev, &d3d11_ibuf_desc, &d3d11_ibuf_data, &d3d11_ibuf);
    assert(SUCCEEDED(hr) && d3d11_ibuf);

    /* create sokol_gfx vertex- and index-buffers with injected D3D11 buffers */
    sg_reset_state_cache();
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .d3d11_buffer = d3d11_vbuf
    });
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(indices),
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .d3d11_buffer = d3d11_ibuf
    });

    /* can release the native D3D11 buffers now, sokol_gfx is holding on to them */
    d3d11_vbuf->lpVtbl->Release(d3d11_vbuf); d3d11_vbuf = 0;
    d3d11_ibuf->lpVtbl->Release(d3d11_ibuf); d3d11_ibuf = 0;

    /* we can inject either a D3D11 texture, or a shader-resource-view, or both */
    D3D11_TEXTURE2D_DESC d3d11_tex_desc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Width = IMG_WIDTH,
        .Height = IMG_HEIGHT,
        .MipLevels = 1,
        .ArraySize = 1,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .Usage = D3D11_USAGE_DYNAMIC,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        .SampleDesc.Count = 1,
    };
    ID3D11Texture2D* d3d11_tex = 0;
    hr = d3d11_dev->lpVtbl->CreateTexture2D(d3d11_dev, &d3d11_tex_desc, 0, &d3d11_tex);
    assert(SUCCEEDED(hr) && d3d11_tex);

    D3D11_SHADER_RESOURCE_VIEW_DESC d3d11_srv_desc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D.MipLevels = 1
    };
    ID3D11ShaderResourceView* d3d11_srv = 0;
    hr = d3d11_dev->lpVtbl->CreateShaderResourceView(d3d11_dev, (ID3D11Resource*)d3d11_tex, &d3d11_srv_desc, &d3d11_srv);
    assert(SUCCEEDED(hr) && d3d11_srv);

    /* and create a sokol_gfx texture with injected D3D11 texture */
    sg_reset_state_cache();
    sg_image_desc img_desc = {
        .usage = SG_USAGE_STREAM,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        // out-comment either of the next lines for testing:
        .d3d11_texture = d3d11_tex,
        .d3d11_shader_resource_view = d3d11_srv
    };
    sg_image img = sg_make_image(&img_desc);
    if (d3d11_tex) {
        d3d11_tex->lpVtbl->Release(d3d11_tex);
        d3d11_tex = 0;
    }
    if (d3d11_srv) {
        d3d11_srv->lpVtbl->Release(d3d11_srv);
        d3d11_srv = 0;
    }

    /* create shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].sem_name = "POSITION",
            [1].sem_name = "TEXCOORD"
        },
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .fs.images[0].image_type = SG_IMAGETYPE_2D,
        .vs.source =
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
            "};\n",
        .fs.source =
            "Texture2D<float4> tex: register(t0);\n"
            "sampler smp: register(s0);\n"
            "float4 main(float2 uv: TEXCOORD0): SV_Target0 {\n"
            "  return tex.Sample(smp, uv);\n"
            "}\n"
    });

    /* pipeline object */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    /* default pass action (clear to gray) */
    sg_pass_action pass_action = { 0 };

    /* resource bindings */
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = img
    };

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)width/(float)height, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    float rx = 0.0f, ry = 0.0f;
    uint32_t counter = 0;
    while (d3d11_process_events()) {
        /* model-view-proj matrix for vertex shader */
        vs_params_t vs_params;
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        /* update texture image with some generated pixel data */
        for (int y = 0; y < IMG_WIDTH; y++) {
            for (int x = 0; x < IMG_HEIGHT; x++) {
                pixels[y * IMG_WIDTH + x] = 0xFF000000 |
                             (counter & 0xFF)<<16 |
                             ((counter*3) & 0xFF)<<8 |
                             ((counter*23) & 0xFF);
                counter++;
            }
        }
        counter++;
        sg_update_image(img, &(sg_image_data){ .subimage[0][0] = SG_RANGE(pixels) });

        /* draw frame */
        sg_begin_default_pass(&pass_action, d3d11_width(), d3d11_height());
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
