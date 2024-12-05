//------------------------------------------------------------------------------
//  dyntex3d-sapp.c
//
//  Test/demo immutable and dynamically updated 3D texture for various
//  texture sizes.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dyntex3d-sapp.glsl.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#include <assert.h> // assert()
#include <string.h> // memset()

#define MIN_WIDTH_HEIGHT (16)
#define MAX_WIDTH_HEIGHT (256)
#define DEPTH (3)
#define RED 0xFF0000FF
#define GREEN 0xFF00FF00
#define BLUE 0xFFFF0000

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_image img;
    sg_bindings bind;
    int width_height;
    bool immutable;
    sgimgui_t sgimgui;
} state = {
    .width_height = 16,
    .immutable = false,
};

static uint32_t pixels[DEPTH * MAX_WIDTH_HEIGHT * MAX_WIDTH_HEIGHT];

static void recreate_image(void);
static void update_pixels(uint64_t frame_count);
static void draw_ui(void);
static sg_range pixels_as_range(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });
    sgimgui_init(&state.sgimgui, &(sgimgui_desc_t){0});

    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(dyntex3d_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .colors[0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        },
    });

    state.img = sg_alloc_image();
    state.bind.images[IMG_tex] = state.img;
    recreate_image();

    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_w = SG_WRAP_CLAMP_TO_EDGE,
    });
}

static void frame(void) {
    if (!state.immutable) {
        update_pixels(sapp_frame_count());
        sg_update_image(state.img, &(sg_image_data){ .subimage[0][0] = pixels_as_range() });
    }
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    for (int slice = 0; slice < 3; slice++) {
        const vs_params_t vs_params = (vs_params_t){ .w = 0.1f + ((float)slice) / 3.0 };
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
        sg_draw(0, 6, 1);
    }
    draw_ui();
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

static void draw_ui(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu(&state.sgimgui, "sokol-gfx");
        igEndMainMenuBar();
    }
    igSetNextWindowPos((ImVec2){20, 40}, ImGuiCond_Once);
    igSetNextWindowSize((ImVec2){220, 150}, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.35f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        if (igSliderIntEx("Size", &state.width_height, MIN_WIDTH_HEIGHT, MAX_WIDTH_HEIGHT, "%d", ImGuiSliderFlags_Logarithmic)) {
            recreate_image();
        }
        if (igCheckbox("Immutable", &state.immutable)) {
            recreate_image();
        }
    }
    igEnd();
    sgimgui_draw(&state.sgimgui);
    simgui_render();
}

static sg_range pixels_as_range(void) {
    return (sg_range){
        .ptr = pixels,
        .size = DEPTH * (size_t)(state.width_height * state.width_height) * sizeof(uint32_t)
    };
}

static void recreate_image(void) {
    if (sg_query_image_state(state.img) == SG_RESOURCESTATE_VALID) {
        sg_uninit_image(state.img);
    }
    update_pixels(sapp_frame_count());
    sg_init_image(state.img, &(sg_image_desc){
        .type = SG_IMAGETYPE_3D,
        .usage = state.immutable ? SG_USAGE_IMMUTABLE : SG_USAGE_STREAM,
        .width = state.width_height,
        .height = state.width_height,
        .num_slices = DEPTH,
        .num_mipmaps = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.subimage[0][0] = state.immutable ? pixels_as_range() : (sg_range){0},
    });
}

// put these into macros instead of functions so we don't get an unused warning in release mode
#define valid_x(x) ((x >= 0) && (x < state.width_height))
#define valid_y(y) ((y >= 0) && (y < state.width_height))
#define valid_z(z) ((z >= 0) && (z < DEPTH))

static inline void set_pixel(int x, int y, int z, uint32_t color) {
    const int wh = state.width_height;
    pixels[z * wh * wh + y * wh + x] = color;
}

static void hori_line(int x0, int y, int z, int len, uint32_t color) {
    assert(valid_x(x0) && valid_y(y) && valid_z(z) && valid_x((x0 + len) - 1));
    for (int x = x0; x < (x0 + len); x++) {
        set_pixel(x, y, z, color);
    }
}

static void vert_line(int x, int y0, int z, int len, uint32_t color) {
    assert(valid_x(x) && valid_y(y0) && valid_z(z) && valid_y((y0 + len) - 1));
    for (int y = y0; y < (y0 + len); y++) {
        set_pixel(x, y, z, color);
    }
}

static void border(int offset, int z, uint32_t color) {
    const int wh = state.width_height;
    hori_line(offset, offset, z, wh - 2 * offset, color);
    hori_line(offset, wh - 1 - offset, z, wh - 2 * offset, color);
    vert_line(offset, offset, z, wh - 2 * offset, color);
    vert_line(wh - 1 - offset, offset, z, wh - 2 * offset, color);
}

static void update_pixels(uint64_t frame_count) {
    memset(pixels, 0, sizeof(pixels));
    static const uint32_t colors[3] = { RED, GREEN, BLUE };
    const int wh = state.width_height;
    for (int i = 0; i < DEPTH; i++) {
        border(i, i, colors[i]);
        int z = i;
        {
            int x = i;
            int len = wh - 2 * i;
            int y = ((int)((frame_count / 8) % (uint64_t)(wh - 2 * i)) + i);
            hori_line(x, y, z, len, colors[i]);
        }
        {
            int y = i;
            int len = wh - 2 * i;
            int x = ((int)((frame_count / 8) % (uint64_t)(wh - 2 * i)) + i);
            vert_line(x, y, z, len, colors[i]);
        }
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .window_title = "dyntex3d-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
