//------------------------------------------------------------------------------
//  drawcallperf-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#define VECMATH_GENERICS
#include "vecmath.h"
#include "drawcallperf-sapp.glsl.h"

#define NUM_IMAGES (3)
#define IMG_WIDTH (8)
#define IMG_HEIGHT (8)
#define MAX_INSTANCES (100000)
#define MAX_BIND_FREQUENCY (1000)

static struct {
    sg_pass_action pass_action;
    sg_image img[NUM_IMAGES];
    sg_view view[NUM_IMAGES];
    sg_pipeline pip;
    sg_bindings bind;
    int num_instances;
    int bind_frequency;
    float angle;
    uint64_t last_time;
    struct {
        int num_uniform_updates;
        int num_binding_updates;
        int num_draw_calls;
    } stats;
    const char* backend;
    sgimgui_t sgimgui;
} state;

static vs_per_instance_t positions[MAX_INSTANCES];

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static vec4_t rand_pos(void) {
    const float x = (((float)(xorshift32() & 0xFFFF)) / 0x10000) - 0.5f;
    const float y = (((float)(xorshift32() & 0xFFFF)) / 0x10000) - 0.5f;
    const float z = (((float)(xorshift32() & 0xFFFF)) / 0x10000) - 0.5f;
    return vm_normalize(vec4(x, y, z, 0.0f));
}

static void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
        .uniform_buffer_size = MAX_INSTANCES * 256 + 1024,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sgimgui_init(&state.sgimgui, &(sgimgui_desc_t){0});
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 0.75f, 1.0f } },
    };
    state.num_instances = 100;
    state.bind_frequency = MAX_BIND_FREQUENCY;

    switch (sg_query_backend()) {
        case SG_BACKEND_GLCORE: state.backend = "GLCORE"; break;
        case SG_BACKEND_GLES3: state.backend = "GLES3"; break;
        case SG_BACKEND_D3D11: state.backend = "D3D11"; break;
        case SG_BACKEND_METAL_IOS: state.backend = "METAL_IOS"; break;
        case SG_BACKEND_METAL_MACOS: state.backend = "METAL_MACOS"; break;
        case SG_BACKEND_METAL_SIMULATOR: state.backend = "METAL_SIMULATOR"; break;
        case SG_BACKEND_WGPU: state.backend = "WGPU"; break;
        case SG_BACKEND_DUMMY: state.backend = "DUMMY"; break;
        default: state.backend = "???"; break;
    }

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
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // three textures and a sampler
    uint32_t pixels[IMG_HEIGHT][IMG_WIDTH];
    for (int i = 0; i < NUM_IMAGES; i++) {
        uint32_t color;
        switch (i) {
            case 0: color = 0xFF0000FF; break;
            case 1: color = 0xFF00FF00; break;
            default: color = 0xFFFF0000; break;
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
        state.view[i] = sg_make_view(&(sg_view_desc){
            .texture = { .image = state.img[i] },
        });
    }
    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_drawcallperf_in_pos] = { .format = SG_VERTEXFORMAT_FLOAT3 },
                [ATTR_drawcallperf_in_uv] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [ATTR_drawcallperf_in_bright] = { .format = SG_VERTEXFORMAT_FLOAT },
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

static mat44_t compute_viewproj(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    state.angle = fmodf(state.angle + 0.01, 360.0f);
    const float dist = 4.5f;
    const vec3_t eye = vec3(vm_sin(state.angle) * dist, 1.5f, vm_cos(state.angle) * dist);
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(eye, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    return vm_mul(view, proj);
}

static void frame(void) {
    double frame_measured_time = stm_sec(stm_laptime(&state.last_time));

    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });

    // sokol-gfx debug ui
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu(&state.sgimgui, "sokol-gfx");
        sgimgui_draw(&state.sgimgui);
        igEndMainMenuBar();
    }

    // control ui
    igSetNextWindowPos((ImVec2){20,20}, ImGuiCond_Once);
    igSetNextWindowSize((ImVec2){600,200}, ImGuiCond_Once);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoResize)) {
        igText("Each cube/instance is 1 16-byte uniform update and 1 draw call\n");
        igText("DC/texture is the number of adjacent draw calls with the same texture binding\n");
        igSliderIntEx("Num Instances", &state.num_instances, 100, MAX_INSTANCES, "%d", ImGuiSliderFlags_Logarithmic);
        igSliderIntEx("DC/texture", &state.bind_frequency, 1, MAX_BIND_FREQUENCY, "%d", ImGuiSliderFlags_Logarithmic);
        igText("Backend: %s", state.backend);
        igText("Frame duration: %.4fms", frame_measured_time * 1000.0);
        igText("sg_apply_bindings(): %d\n", state.stats.num_binding_updates);
        igText("sg_apply_uniforms(): %d\n", state.stats.num_uniform_updates);
        igText("sg_draw(): %d\n", state.stats.num_draw_calls);
    }
    igEnd();

    if (state.num_instances < 1) {
        state.num_instances = 1;
    } else if (state.num_instances > MAX_INSTANCES) {
        state.num_instances = MAX_INSTANCES;
    }

    // view-proj matrix for the frame
    const vs_per_frame_t vs_per_frame = {
        .viewproj = compute_viewproj(),
    };

    state.stats.num_uniform_updates = 0;
    state.stats.num_binding_updates = 0;
    state.stats.num_draw_calls = 0;

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(UB_vs_per_frame, &SG_RANGE(vs_per_frame));
    state.stats.num_uniform_updates++;

    state.bind.views[VIEW_tex] = state.view[0];
    sg_apply_bindings(&state.bind);
    state.stats.num_binding_updates++;
    int cur_bind_count = 0;
    int cur_img = 0;
    for (int i = 0; i < state.num_instances; i++) {
        if (++cur_bind_count == state.bind_frequency) {
            cur_bind_count = 0;
            if (cur_img == NUM_IMAGES) {
                cur_img = 0;
            }
            state.bind.views[VIEW_tex] = state.view[cur_img++];
            sg_apply_bindings(&state.bind);
            state.stats.num_binding_updates++;
        }
        sg_apply_uniforms(UB_vs_per_instance, &SG_RANGE(positions[i]));
        state.stats.num_uniform_updates++;
        sg_draw(0, 36, 1);
        state.stats.num_draw_calls++;
    }
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

static void cleanup(void) {
    sgimgui_discard(&state.sgimgui);
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
