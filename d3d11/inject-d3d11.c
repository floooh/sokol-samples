//------------------------------------------------------------------------------
//  inject-d3d11.c
//  Demonstrate usage of injected native D3D11 buffer and image resources.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"
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
    mat44_t mvp;
} vs_params_t;

static mat44_t compute_mvp(float rx, float ry, int width, int height) {
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)width/(float)height, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, view_proj);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d11 app wrapper and sokol_gfx
    d3d11_init(&(d3d11_desc_t){ .width = 640, .height = 480, .sample_count = 4, .title = L"inject-d3d11.c" });
    sg_setup(&(sg_desc){
        .environment = d3d11_environment(),
        .logger.func = slog_func,
    });

    // create native D3D11 vertex and index buffers
    float vertices[] = {
        // pos                  uvs
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
    ID3D11Device* d3d11_dev = (ID3D11Device*) sg_d3d11_device();
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

    // create sokol_gfx vertex- and index-buffers with injected D3D11 buffers
    sg_reset_state_cache();
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .d3d11_buffer = d3d11_vbuf
    });
    assert(sg_d3d11_query_buffer_info(vbuf).buf == d3d11_vbuf);
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .size = sizeof(indices),
        .d3d11_buffer = d3d11_ibuf
    });
    assert(sg_d3d11_query_buffer_info(ibuf).buf == d3d11_ibuf);

    // can release the native D3D11 buffers now, sokol_gfx is holding on to them
    d3d11_vbuf->lpVtbl->Release(d3d11_vbuf); d3d11_vbuf = 0;
    d3d11_ibuf->lpVtbl->Release(d3d11_ibuf); d3d11_ibuf = 0;

    // we can inject either a D3D11 texture, or a shader-resource-view, or both
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

    // and create a sokol_gfx texture with injected D3D11 texture
    sg_reset_state_cache();
    sg_image img = sg_make_image(&(sg_image_desc){
        .usage.stream_update = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .d3d11_texture = d3d11_tex,
    });
    assert(sg_d3d11_query_image_info(img).tex2d == d3d11_tex);
    assert(sg_d3d11_query_image_info(img).tex3d == 0);
    assert(sg_d3d11_query_image_info(img).res == d3d11_tex);
    if (d3d11_tex) {
        d3d11_tex->lpVtbl->Release(d3d11_tex);
        d3d11_tex = 0;
    }

    // note: view objects can currently not be injected
    sg_view tex_view = sg_make_view(&(sg_view_desc){ .texture.image = img });

    // create a D3D11 sampler and inject into an sg_sampler
    D3D11_SAMPLER_DESC d3d11_smp_desc = {
        .Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
        .MaxAnisotropy = 1,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    ID3D11SamplerState* d3d11_smp;
    hr = d3d11_dev->lpVtbl->CreateSamplerState(d3d11_dev, &d3d11_smp_desc, &d3d11_smp);
    assert(SUCCEEDED(hr) && d3d11_smp);
    sg_reset_state_cache();
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .d3d11_sampler = d3d11_smp,
    });
    assert(sg_d3d11_query_sampler_info(smp).smp == d3d11_smp);
    d3d11_smp->lpVtbl->Release(d3d11_smp); d3d11_smp = 0;

    // create shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
        .fragment_func.source =
            "Texture2D<float4> tex: register(t0);\n"
            "sampler smp: register(s0);\n"
            "float4 main(float2 uv: TEXCOORD0): SV_Target0 {\n"
            "  return tex.Sample(smp, uv);\n"
            "}\n",
        .attrs = {
            [0].hlsl_sem_name = "POSITION",
            [1].hlsl_sem_name = "TEXCOORD"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .hlsl_register_b_n = 0,
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .hlsl_register_t_n = 0,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .hlsl_register_s_n = 0,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
        },
    });
    assert(sg_d3d11_query_shader_info(shd).vs != 0);
    assert(sg_d3d11_query_shader_info(shd).fs != 0);
    assert(sg_d3d11_query_shader_info(shd).cbufs[0] != 0);
    assert(sg_d3d11_query_shader_info(shd).cbufs[1] == 0);

    // pipeline object
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
    assert(sg_d3d11_query_pipeline_info(pip).il != 0);
    assert(sg_d3d11_query_pipeline_info(pip).rs != 0);
    assert(sg_d3d11_query_pipeline_info(pip).dss != 0);
    assert(sg_d3d11_query_pipeline_info(pip).bs != 0);

    // default pass action (clear to gray)
    sg_pass_action pass_action = { 0 };

    // resource bindings
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .views[0] = tex_view,
        .samplers[0] = smp,
    };

    float rx = 0.0f, ry = 0.0f;
    uint32_t counter = 0;
    while (d3d11_process_events()) {
        // model-view-proj matrix for vertex shader
        rx += 1.0f; ry += 2.0f;
        const vs_params_t vs_params = { .mvp = compute_mvp(rx, ry, d3d11_width(), d3d11_height()) };

        // update texture image with some generated pixel data
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
        sg_update_image(img, &(sg_image_data){ .mip_levels[0] = SG_RANGE(pixels) });

        // draw frame
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = d3d11_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
