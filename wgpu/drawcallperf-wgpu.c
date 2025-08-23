//------------------------------------------------------------------------------
//  drawcallperf-sapp.c
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_time.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#define SOKOL_IMGUI_NO_SOKOL_APP
#include "sokol_imgui.h"

#define NUM_IMAGES (3)
#define IMG_WIDTH (8)
#define IMG_HEIGHT (8)
#define MAX_INSTANCES (100000)
#define MAX_BIND_FREQUENCY (1000)

static struct {
    sg_pass_action pass_action;
    sg_view tex_view[NUM_IMAGES];
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
} state;

typedef struct vs_per_frame_t {
    hmm_mat4 viewproj;
} vs_per_frame_t;

typedef struct vs_per_instance_t {
    hmm_vec4 world_pos;
} vs_per_instance_t;

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

static void ig_mouse_pos(float x, float y) {
    igGetIO()->MousePos = (ImVec2){ x, y };
}

static void ig_mouse_btn_down(int btn) {
    igGetIO()->MouseDown[btn] = true;
}

static void ig_mouse_btn_up(int btn) {
    igGetIO()->MouseDown[btn] = false;
}

static void ig_mouse_wheel(float v) {
    igGetIO()->MouseWheel = v;
}

static void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){
        .environment = wgpu_environment(),
        .logger.func = slog_func,
        .uniform_buffer_size = MAX_INSTANCES * 256 + 1024,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });

    // input forwarding
    wgpu_mouse_pos(ig_mouse_pos);
    wgpu_mouse_btn_down(ig_mouse_btn_down);
    wgpu_mouse_btn_up(ig_mouse_btn_up);
    wgpu_mouse_wheel(ig_mouse_wheel);

    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 0.75f, 1.0f } },
    };
    state.num_instances = 100;
    state.bind_frequency = MAX_BIND_FREQUENCY;

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
        state.tex_view[i] = sg_make_view(&(sg_view_desc){
            .texture.image = sg_make_image(&(sg_image_desc){
                .width = IMG_WIDTH,
                .height = IMG_HEIGHT,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .data.subimage[0][0] = SG_RANGE(pixels),
            }),
        });
    }
    state.bind.samplers[0] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // a shader object
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct vs_per_frame {\n"
            "  viewproj: mat4x4f,\n"
            "}\n"
            "struct vs_per_instance {\n"
            "  world_pos: vec4f,\n"
            "}\n"
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) uv: vec2f,\n"
            "  @location(1) bright: f32,\n"
            "}\n"
            "@group(0) @binding(0) var<uniform> per_frame: vs_per_frame;\n"
            "@group(0) @binding(1) var<uniform> per_instance: vs_per_instance;\n"
            "@vertex fn main(@location(0) pos: vec3f, @location(1) uv: vec2f, @location(2) bright: f32) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = per_frame.viewproj * (per_instance.world_pos + vec4f(pos * 0.05, 1.0));\n"
            "  out.uv = uv;\n"
            "  out.bright = bright;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "@group(1) @binding(0) var tex: texture_2d<f32>;\n"
            "@group(1) @binding(1) var smp: sampler;\n"
            "@fragment fn main(@location(0) uv: vec2f, @location(1) bright: f32) -> @location(0) vec4f {\n"
            "  return vec4f(textureSample(tex, smp, uv).xyz * bright, 1.0);\n"
            "}\n",
        .uniform_blocks = {
            [0] = {
                .stage = SG_SHADERSTAGE_VERTEX,
                .size = sizeof(vs_per_frame_t),
                .wgsl_group0_binding_n = 0,
            },
            [1] = {
                .stage = SG_SHADERSTAGE_VERTEX,
                .size = sizeof(vs_per_instance_t),
                .wgsl_group0_binding_n = 1,
            }
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .wgsl_group1_binding_n = 0,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .wgsl_group1_binding_n = 1,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0
        },
    });

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [2] = { .format = SG_VERTEXFORMAT_FLOAT },
            }
        },
        .shader = shd,
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

static hmm_mat4 compute_viewproj(void) {
    const float w = (float)wgpu_width();
    const float h = (float)wgpu_width();
    state.angle = fmodf(state.angle + 0.01, 360.0f);
    const float dist = 6.0f;
    const hmm_vec3 eye = HMM_Vec3(HMM_SinF(state.angle) * dist, 1.5f, HMM_CosF(state.angle) * dist);
    hmm_mat4 proj = HMM_Perspective(60.0f, w/h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(eye, HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    return HMM_MultiplyMat4(proj, view);
}

static void frame(void) {
    double frame_measured_time = stm_sec(stm_laptime(&state.last_time));

    simgui_new_frame(&(simgui_frame_desc_t){
        .width = wgpu_width(),
        .height = wgpu_height(),
        .delta_time = frame_measured_time,
        .dpi_scale = 1.0f,
    });

    // control ui
    igSetNextWindowPos((ImVec2){20,20}, ImGuiCond_Once);
    igSetNextWindowSize((ImVec2){600,200}, ImGuiCond_Once);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoResize)) {
        igText("Each cube/instance is 1 16-byte uniform update and 1 draw call\n");
        igText("DC/texture is the number of adjacent draw calls with the same texture binding\n");
        igSliderIntEx("Num Instances", &state.num_instances, 100, MAX_INSTANCES, "%d", ImGuiSliderFlags_Logarithmic);
        igSliderIntEx("DC/texture", &state.bind_frequency, 1, MAX_BIND_FREQUENCY, "%d", ImGuiSliderFlags_Logarithmic);
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

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = wgpu_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(0, &SG_RANGE(vs_per_frame));
    state.stats.num_uniform_updates++;

    state.bind.views[0] = state.tex_view[0];
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
            state.bind.views[0] = state.tex_view[cur_img++];
            sg_apply_bindings(&state.bind);
            state.stats.num_binding_updates++;
        }
        sg_apply_uniforms(1, &SG_RANGE(positions[i]));
        state.stats.num_uniform_updates++;
        sg_draw(0, 36, 1);
        state.stats.num_draw_calls++;
    }
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    simgui_shutdown();
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = 1,
        .width = 1024,
        .height = 768,
        .title = "drawcallperf-wgpu.c",
    });
    return 0;
}
