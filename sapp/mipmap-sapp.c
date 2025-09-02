//------------------------------------------------------------------------------
//  mipmap-sapp.c
//  Demonstrate all the mipmapping filters.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "dbgui/dbgui.h"
#include "mipmap-sapp.glsl.h"
#include <assert.h>

static struct {
    sg_pipeline pip;
    sg_buffer vbuf;
    sg_view tex_view;
    sg_sampler smp[12];
    float r;
    struct {
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
} state;

static const uint32_t mip_colors[9] = {
    0xFF0000FF,     // red
    0xFF00FF00,     // green
    0xFFFF0000,     // blue
    0xFFFF00FF,     // magenta
    0xFFFFFF00,     // cyan
    0xFF00FFFF,     // yellow
    0xFFFF00A0,     // violet
    0xFFFFA0FF,     // orange
    0xFFA000FF,     // purple
};

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // a plane vertex buffer
    float vertices[] = {
        -1.0, -1.0, 0.0,  0.0, 0.0,
        +1.0, -1.0, 0.0,  1.0, 0.0,
        -1.0, +1.0, 0.0,  0.0, 1.0,
        +1.0, +1.0, 0.0,  1.0, 1.0,
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // create image with mipmap content: different colors and checkboard pattern
    sg_image_data img_data;
    uint32_t* ptr = state.pixels.mip0;
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
    sg_image img = sg_make_image(&(sg_image_desc){
        .width = 256,
        .height = 256,
        .num_mipmaps = 9,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data = img_data
    });

    // ...and a texture view for the image
    state.tex_view = sg_make_view(&(sg_view_desc){ .texture = { .image = img } });

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

    // pipeline state
    state.pip = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = {
            .attrs = {
                [ATTR_mipmap_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_mipmap_uv0].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = sg_make_shader(mipmap_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });
}

static void frame(void) {
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(90.0f), sapp_widthf()/sapp_heightf(), 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, 3.5f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    state.r += 0.1f * 60.0f * (float)sapp_frame_duration();
    const mat44_t rm = mat44_rotation_x(vm_radians(state.r));

    sg_bindings bind = {
        .vertex_buffers[0] = state.vbuf,
        .views[VIEW_tex] = state.tex_view,
    };
    sg_begin_pass(&(sg_pass){ .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    for (int i = 0; i < 12; i++) {
        const float x = ((float)(i & 3) - 1.5f) * 2.0f;
        const float y = ((float)(i / 4) - 1.0f) * -2.0f;
        const mat44_t model = vm_mul(rm, mat44_translation(x, y, 0.0f));
        const vs_params_t vs_params= { .mvp = vm_mul(model, view_proj) };

        bind.samplers[SMP_smp] = state.smp[i];
        sg_apply_bindings(&bind);
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
        sg_draw(0, 4, 1);
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "mipmap-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
