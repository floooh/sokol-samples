//------------------------------------------------------------------------------
//  dyntex3d-sapp.c
//
//  Test/demo dynamocally updated 3D texture.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "dyntex3d-sapp.glsl.h"
#include <assert.h> // assert()
#include <string.h> // memset()

#define WIDTH (32)
#define HEIGHT (32)
#define DEPTH (3)
#define RED 0xFF0000FF
#define GREEN 0xFF00FF00
#define BLUE 0xFFFF0000

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_image img;
    sg_bindings bind;
} state;

static uint32_t pixels[DEPTH][HEIGHT][WIDTH];

static void update_pixels(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    // don't need a vertex and index buffer, we'll just render a simple rectangle

    // create shader and pipeline object
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
        .label = "pipeline",
    });

    // create a dynamically updated 3D texture
    state.img = sg_make_image(&(sg_image_desc){
        .type = SG_IMAGETYPE_3D,
        .usage = SG_USAGE_STREAM,
        .width = WIDTH,
        .height = HEIGHT,
        .num_slices = DEPTH,
        .num_mipmaps = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "3d texture",
    });
    state.bind.fs.images[SLOT_tex] = state.img;

    // ... and a matching sampler
    state.bind.fs.samplers[SLOT_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_w = SG_WRAP_CLAMP_TO_EDGE,
    });
}

static void frame(void) {
    update_pixels();
    sg_update_image(state.img, &(sg_image_data){ .subimage[0][0] = SG_RANGE(pixels) });

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    for (int slice = 0; slice < 3; slice++) {
        const vs_params_t vs_params = (vs_params_t){ .w = 0.1f + ((float)slice) / 3.0 };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
        sg_draw(0, 6, 1);
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

// put these into macros instead of functions so we don't get an unused warning in release mode
#define valid_x(x) ((x >= 0) && (x < WIDTH))
#define valid_y(y) ((y >= 0) && (y < HEIGHT))
#define valid_z(z) ((z >= 0) && (z < DEPTH))

static void hori_line(int x0, int y, int z, int len, uint32_t color) {
    assert(valid_x(x0) && valid_y(y) && valid_z(z) && valid_x((x0 + len) - 1));
    for (int x = x0; x < (x0 + len); x++) {
        pixels[z][y][x] = color;
    }
}

static void vert_line(int x, int y0, int z, int len, uint32_t color) {
    assert(valid_x(x) && valid_y(y0) && valid_z(z) && valid_y((y0 + len) - 1));
    for (int y = y0; y < (y0 + len); y++) {
        pixels[z][y][x] = color;
    }
}

static void border(int offset, int z, uint32_t color) {
    hori_line(offset, offset, z, WIDTH - 2 * offset, color);
    hori_line(offset, HEIGHT - 1 - offset, z, WIDTH - 2 * offset, color);
    vert_line(offset, offset, z, HEIGHT - 2 * offset, color);
    vert_line(WIDTH - 1 - offset, offset, z, HEIGHT - 2 * offset, color);
}

static void update_pixels(void) {
    memset(pixels, 0, sizeof(pixels));

    static const uint32_t colors[3] = { RED, GREEN, BLUE };
    for (int i = 0; i < DEPTH; i++) {
        border(i, i, colors[i]);
        int z = i;
        {
            int x = i;
            int len = WIDTH - 2 * i;
            int y = ((int)((sapp_frame_count() / 16) % (uint64_t)(HEIGHT - 2 * i)) + i);
            hori_line(x, y, z, len, colors[i]);
        }
        {
            int y = i;
            int len = HEIGHT - 2 * i;
            int x = ((int)((sapp_frame_count() / 16) % (uint64_t)(WIDTH - 2 * i)) + i);
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
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .window_title = "dyntex3d-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
