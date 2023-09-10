//------------------------------------------------------------------------------
//  drawcallperf-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "drawcallperf-sapp.glsl.h"

#define NUM_IMAGES (3)
#define IMG_WIDTH (8)
#define IMG_HEIGHT (8)
#define MAX_INSTANCES (128 * 1024)

static struct {
    sg_pass_action pass_action;
    sg_image img[NUM_IMAGES];
    sg_pipeline pip;
    sg_bindings bind;
    int num_instances;
} state;

static vs_per_instance_t positions[MAX_INSTANCES];

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static hmm_vec4 rand_pos(void) {
    const float x = (((float)(xorshift32() & 0xFFFF)) / 0x10000) - 0.5f;
    const float y = (((float)(xorshift32() & 0xFFFF)) / 0x10000) - 0.5f;
    const float z = (((float)(xorshift32() & 0xFFFF)) / 0x10000) - 0.5f;
    return HMM_NormalizeVec4(HMM_Vec4(x, y, z, 0.0f));
}

static hmm_mat4 compute_viewproj(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
//    const float t = (float)(sapp_frame_duration() * 60.0);
    hmm_mat4 proj = HMM_Perspective(60.0f, w/h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    return HMM_MultiplyMat4(proj, view);
}

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 0.75f, 1.0f } },
    };
    state.num_instances = 100;

    // vertices and indices for a 2d quad
    static const float vertices[] = {
        -1.0, -1.0, -1.0,   0.0, 0.0,  1.0,
         1.0, -1.0, -1.0,   1.0, 0.0,  1.0,
         1.0,  1.0, -1.0,   1.0, 1.0,  1.0,
        -1.0,  1.0, -1.0,   0.0, 1.0,  1.0,

        -1.0, -1.0,  1.0,   0.0, 0.0,  0.9,
         1.0, -1.0,  1.0,   1.0, 0.0,  0.9,
         1.0,  1.0,  1.0,   1.0, 1.0,  0.9,
        -1.0,  1.0,  1.0,   0.0, 1.0,  0.9,

        -1.0, -1.0, -1.0,   0.0, 0.0,  0.8,
        -1.0,  1.0, -1.0,   1.0, 0.0,  0.8,
        -1.0,  1.0,  1.0,   1.0, 1.0,  0.8,
        -1.0, -1.0,  1.0,   0.0, 1.0,  0.8,

        1.0, -1.0, -1.0,    0.0, 0.0,  0.7,
        1.0,  1.0, -1.0,    1.0, 0.0,  0.7,
        1.0,  1.0,  1.0,    1.0, 1.0,  0.7,
        1.0, -1.0,  1.0,    0.0, 1.0,  0.7,

        -1.0, -1.0, -1.0,   0.0, 0.0,  0.6,
        -1.0, -1.0,  1.0,   1.0, 0.0,  0.6,
         1.0, -1.0,  1.0,   1.0, 1.0,  0.6,
         1.0, -1.0, -1.0,   0.0, 1.0,  0.6,

        -1.0,  1.0, -1.0,   0.0, 0.0,  0.5,
        -1.0,  1.0,  1.0,   1.0, 0.0,  0.5,
         1.0,  1.0,  1.0,   1.0, 1.0,  0.5,
         1.0,  1.0, -1.0,   0.0, 1.0,  0.5,
    };
    static const uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    // three textures and a sampler
    uint32_t pixels[IMG_HEIGHT][IMG_WIDTH];
    for (int i = 0; i < NUM_IMAGES; i++) {
        uint32_t color;
        switch (i) {
            case 0: color = 0x0000FFFF; break;
            case 1: color = 0x00FF00FF; break;
            default: color = 0xFF0000FF; break;
        }
        for (int y = 0; y < IMG_WIDTH; y++) {
            for (int x = 0; x < IMG_WIDTH; x++) {
                pixels[y][x] = color;
            }
        }
        state.img[i] = sg_make_image(&(sg_image_desc){
            .width = IMG_WIDTH,
            .height = IMG_HEIGHT,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data.subimage[0][0] = SG_RANGE(pixels),
        });
    }
    state.bind.fs.samplers[SLOT_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_vs_in_pos] = { .format = SG_VERTEXFORMAT_FLOAT3 },
                [ATTR_vs_in_uv] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [ATTR_vs_in_bright] = { .format = SG_VERTEXFORMAT_FLOAT },
            }
        },
        .shader = sg_make_shader(drawcallperf_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
    });

    // initialize a fixed array of random positions
    for (int i = 0; i < MAX_INSTANCES; i++) {
        positions[i].world_pos = rand_pos();
    }
}

static void frame(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });

    // view-proj matrix for the frame
    const vs_per_frame_t vs_per_frame = {
        .viewproj = compute_viewproj(),
    };

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_per_frame, &SG_RANGE(vs_per_frame));
    // FIXME: move into loop with 'bind frequency'
    state.bind.fs.images[SLOT_tex] = state.img[0];
    sg_apply_bindings(&state.bind);
    for (int i = 0; i < state.num_instances; i++) {
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_per_instance, &SG_RANGE(positions[i]));
        sg_draw(0, 36, 1);
    }
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

static void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 1024,
        .height = 768,
        .sample_count = 4,
        .window_title = "drawcallperf-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
