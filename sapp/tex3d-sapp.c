//------------------------------------------------------------------------------
//  tex3d-sapp.c
//  Test 3D texture rendering.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "tex3d-sapp.glsl.h"

#define TEX3D_DIM (32)

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry, t;
} state;

static uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.25f, 0.5f, 0.75f, 1.0f } }
    };

    // build cube vertex- and index-buffer
    const float vertices[] = {
        -1.0, -1.0, -1.0,    1.0, -1.0, -1.0,    1.0,  1.0, -1.0,   -1.0,  1.0, -1.0,
        -1.0, -1.0,  1.0,    1.0, -1.0,  1.0,    1.0,  1.0,  1.0,   -1.0,  1.0,  1.0,
        -1.0, -1.0, -1.0,   -1.0,  1.0, -1.0,   -1.0,  1.0,  1.0,   -1.0, -1.0,  1.0,
         1.0, -1.0, -1.0,    1.0,  1.0, -1.0,    1.0,  1.0,  1.0,    1.0, -1.0,  1.0,
        -1.0, -1.0, -1.0,   -1.0, -1.0,  1.0,    1.0, -1.0,  1.0,    1.0, -1.0, -1.0,
        -1.0,  1.0, -1.0,   -1.0,  1.0,  1.0,    1.0,  1.0,  1.0,    1.0,  1.0, -1.0,
    };
    const uint16_t indices[] = {
        0, 1, 2,     0, 2, 3,
        6, 5, 4,     7, 6, 4,
        8, 9, 10,    8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage.vertex_buffer = true,
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // create shader and pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3
            }
        },
        .shader = sg_make_shader(cube_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "cube-pipeline"
    });

    // create a 3d texture with random content
    static uint32_t pixels[TEX3D_DIM][TEX3D_DIM][TEX3D_DIM];
    for (int x = 0; x < TEX3D_DIM; x++) {
        for (int y = 0; y < TEX3D_DIM; y++) {
            for (int z = 0; z < TEX3D_DIM; z++) {
                pixels[x][y][z] = xorshift32();
            }
        }
    }
    state.bind.images[IMG_tex] = sg_make_image(&(sg_image_desc){
        .type = SG_IMAGETYPE_3D,
        .width = TEX3D_DIM,
        .height = TEX3D_DIM,
        .num_slices = TEX3D_DIM,
        .num_mipmaps = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "3d texture",
        .data.subimage[0][0] = SG_RANGE(pixels)
    });

    // ...and a sampler object
    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });
}

static void frame(void) {
    // compute vertex-shader params (mvp and texcoord-scale)
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t;
    state.ry += 2.0f * t;
    state.t  += 0.03f * t;
    hmm_mat4 proj = HMM_Perspective(60.0f, sapp_widthf()/sapp_heightf(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params_t vs_params = {
        .mvp = HMM_MultiplyMat4(view_proj, model),
        .scale = (HMM_SinF(state.t) + 1.0f) * 0.5f
    };

    // render the scene
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
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
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "3D Texture Rendering",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
