//------------------------------------------------------------------------------
//  inject-d3d12.c
//  Demonstrate usage of injected native D3D12 buffer and image resources.
//------------------------------------------------------------------------------
#include "d3d12entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D12
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"
#define D3D12_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>

// IID for ID3D12Resource (avoids linking dxguid.lib)
static const IID _inject_IID_ID3D12Resource = { 0x696442be,0xa72e,0x4059, {0xbc,0x79,0x5b,0x5c,0x98,0x04,0x0f,0xad} };

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
    // setup d3d12 app wrapper and sokol_gfx
    d3d12_init(&(d3d12_desc_t){ .width = 640, .height = 480, .sample_count = 4, .title = L"inject-d3d12.c" });
    sg_setup(&(sg_desc){
        .environment = d3d12_environment(),
        .logger.func = slog_func,
    });

    // create native D3D12 vertex and index buffers
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

    ID3D12Device* d3d12_dev = (ID3D12Device*) d3d12_get_device();

    // create D3D12 vertex buffer (committed resource in upload heap for simplicity)
    D3D12_HEAP_PROPERTIES vbuf_heap_props = {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
    };
    D3D12_RESOURCE_DESC vbuf_res_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(vertices),
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc.Count = 1,
        .SampleDesc.Quality = 0,
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };
    ID3D12Resource* d3d12_vbuf = 0;
    HRESULT hr = d3d12_dev->lpVtbl->CreateCommittedResource(d3d12_dev,
        &vbuf_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &vbuf_res_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        0,
        &_inject_IID_ID3D12Resource,
        (void**)&d3d12_vbuf);
    assert(SUCCEEDED(hr) && d3d12_vbuf);

    // copy vertex data
    void* vbuf_mapped = 0;
    D3D12_RANGE read_range = { 0, 0 };
    hr = d3d12_vbuf->lpVtbl->Map(d3d12_vbuf, 0, &read_range, &vbuf_mapped);
    assert(SUCCEEDED(hr));
    memcpy(vbuf_mapped, vertices, sizeof(vertices));
    d3d12_vbuf->lpVtbl->Unmap(d3d12_vbuf, 0, 0);

    // create D3D12 index buffer
    D3D12_HEAP_PROPERTIES ibuf_heap_props = {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
    };
    D3D12_RESOURCE_DESC ibuf_res_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(indices),
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc.Count = 1,
        .SampleDesc.Quality = 0,
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };
    ID3D12Resource* d3d12_ibuf = 0;
    hr = d3d12_dev->lpVtbl->CreateCommittedResource(d3d12_dev,
        &ibuf_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &ibuf_res_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        0,
        &_inject_IID_ID3D12Resource,
        (void**)&d3d12_ibuf);
    assert(SUCCEEDED(hr) && d3d12_ibuf);

    // copy index data
    void* ibuf_mapped = 0;
    hr = d3d12_ibuf->lpVtbl->Map(d3d12_ibuf, 0, &read_range, &ibuf_mapped);
    assert(SUCCEEDED(hr));
    memcpy(ibuf_mapped, indices, sizeof(indices));
    d3d12_ibuf->lpVtbl->Unmap(d3d12_ibuf, 0, 0);

    // create sokol_gfx vertex- and index-buffers with injected D3D12 buffers
    sg_reset_state_cache();
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .d3d12_buffer = d3d12_vbuf
    });
    assert(sg_d3d12_query_buffer_info(vbuf).res == d3d12_vbuf);
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .size = sizeof(indices),
        .d3d12_buffer = d3d12_ibuf
    });
    assert(sg_d3d12_query_buffer_info(ibuf).res == d3d12_ibuf);

    // can release the native D3D12 buffers now, sokol_gfx is holding on to them
    d3d12_vbuf->lpVtbl->Release(d3d12_vbuf); d3d12_vbuf = 0;
    d3d12_ibuf->lpVtbl->Release(d3d12_ibuf); d3d12_ibuf = 0;

    // create a D3D12 texture (for dynamic textures we just use sokol's approach)
    // Note: D3D12 texture injection is more complex due to resource states and upload heaps
    // For this sample we'll create the texture through sokol and update it dynamically
    sg_image img = sg_make_image(&(sg_image_desc){
        .usage.stream_update = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
    });

    // note: view objects can currently not be injected
    sg_view tex_view = sg_make_view(&(sg_view_desc){ .texture.image = img });

    // create a sampler (D3D12 samplers are in descriptor heaps, not standalone objects)
    // Use sokol's sampler creation which handles the D3D12 descriptor heap internally
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
    });

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
    while (d3d12_process_events()) {
        // model-view-proj matrix for vertex shader
        rx += 1.0f; ry += 2.0f;
        const vs_params_t vs_params = { .mvp = compute_mvp(rx, ry, d3d12_width(), d3d12_height()) };

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
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = d3d12_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        d3d12_present();
    }
    sg_shutdown();
    d3d12_shutdown();
    return 0;
}
