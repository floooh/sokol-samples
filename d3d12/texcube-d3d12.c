//------------------------------------------------------------------------------
//  texcube-d3d12.c
//  Texture creation and rendering.
//------------------------------------------------------------------------------
#include "d3d12entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D12
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

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
    d3d12_init(&(d3d12_desc_t){ .width = 800, .height = 600, .sample_count = 4, .title = L"texcube-d3d12.c" });
    sg_setup(&(sg_desc){
        .environment = d3d12_environment(),
        .logger.func = slog_func,
    });

    // cube vertex buffer
    float vertices[] = {
        // pos                  color                       uvs
        -1.0f, -1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     0.0f, 1.0f,

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

    // create an index buffer for the cube
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

    // create a checkerboard image and texture view
    uint32_t pixels[4*4] = {
        0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000,
        0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
        0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000,
        0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    };
    sg_image img = sg_make_image(&(sg_image_desc){
        .width = 4,
        .height = 4,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.mip_levels[0] = SG_RANGE(pixels)
    });
    sg_view tex_view = sg_make_view(&(sg_view_desc){ .texture.image = img });

    // create a sampler
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    // create shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "cbuffer params: register(b0) {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 pos: POSITION;\n"
            "  float4 color: COLOR1;\n"
            "  float2 uv: TEXCOORD1;\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 color: COLOR0;\n"
            "  float2 uv: TEXCOORD0;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = mul(mvp, inp.pos);\n"
            "  outp.color = inp.color;\n"
            "  outp.uv = inp.uv * 5.0;\n"
            "  return outp;\n"
            "};\n",
        .fragment_func.source =
            "Texture2D<float4> tex: register(t0);\n"
            "sampler smp: register(s0);\n"
            "float4 main(float4 color: COLOR0, float2 uv: TEXCOORD0): SV_Target0 {\n"
            "  return tex.Sample(smp, uv) * color;\n"
            "}\n",
        .attrs = {
            [0] = { .hlsl_sem_name="POSITION" },
            [1] = { .hlsl_sem_name="COLOR", .hlsl_sem_index=1 },
            [2] = { .hlsl_sem_name="TEXCOORD", .hlsl_sem_index=1 }
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
                [1].format=SG_VERTEXFORMAT_FLOAT4,
                [2].format=SG_VERTEXFORMAT_FLOAT2
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
    const sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .views[0] = tex_view,
        .samplers[0] = smp,
    };

    float rx = 0.0f, ry = 0.0f;
    while (d3d12_process_events()) {
        rx += 1.0f; ry += 2.0f;
        const vs_params_t vs_params = { .mvp = compute_mvp(rx, ry, d3d12_width(), d3d12_height()) };
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
