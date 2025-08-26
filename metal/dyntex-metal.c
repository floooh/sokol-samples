//------------------------------------------------------------------------------
//  dyntex-metal.c
//------------------------------------------------------------------------------
#include <stdlib.h> // rand
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

#define DISPLAY_WIDTH (640)
#define DISPLAY_HEIGHT (480)
#define SAMPLE_COUNT (4)
#define IMAGE_WIDTH (64)
#define IMAGE_HEIGHT (64)
#define LIVING (0xFFFFFFFF)
#define DEAD (0xFF000000)

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry;
    int update_count;
    uint32_t pixels[IMAGE_WIDTH][IMAGE_HEIGHT];
} state;

typedef struct {
    mat44_t mvp;
} vs_params_t;

static void game_of_life_init();
static void game_of_life_update();
static mat44_t compute_mvp(float rx, float ry, int width, int height);

static void init(void) {
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    // a 128x128 image with streaming update strategy
    state.img = sg_make_image(&(sg_image_desc){
        .usage.stream_update = true,
        .width = IMAGE_WIDTH,
        .height = IMAGE_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
    });

    // a texture view to bind the image as texture
    state.bind.views[0] = sg_make_view(&(sg_view_desc){ .texture.image = state.img });

    // ...and a sampler
    state.bind.samplers[0] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
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
    uint16_t indices[] = {
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

    // a shader to render a textured cube
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float4 position [[attribute(0)]];\n"
            "  float4 color [[attribute(1)]];\n"
            "  float2 uv [[attribute(2)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float4 color;\n"
            "  float2 uv;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  out.pos = params.mvp * in.position;\n"
            "  out.color = in.color;\n"
            "  out.uv = in.uv;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct fs_in {\n"
            "  float4 color;\n"
            "  float2 uv;\n"
            "};\n"
            "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
            "  return float4(tex.sample(smp, in.uv).xyz, 1.0) * in.color;\n"
            "};\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .msl_texture_n = 0,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .msl_sampler_n = 0,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
        },
    });

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 },
                [2] = { .format=SG_VERTEXFORMAT_FLOAT2 }
            },
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    // initialize the game-of-life state
    game_of_life_init();
}

static void frame(void) {
    // compute model-view-projection matrix
    state.rx += 0.1f; state.ry += 0.2f;
    const vs_params_t vs_params = { .mvp = compute_mvp(state.rx, state.ry, osx_width(), osx_height()) };

    // update game-of-life state
    game_of_life_update();

    // update the texture
    sg_update_image(state.img, &(sg_image_data){
        .subimage[0][0] = SG_RANGE(state.pixels)
    });

    // render the frame
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = osx_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(DISPLAY_WIDTH, DISPLAY_HEIGHT, SAMPLE_COUNT, SG_PIXELFORMAT_DEPTH, "dyntex-metal", init, frame, shutdown);
    return 0;
}

static mat44_t compute_mvp(float rx, float ry, int width, int height) {
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)width/(float)height, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, view_proj);
}

static void game_of_life_init() {
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            if ((rand() & 255) > 230) {
                state.pixels[y][x] = LIVING;
            } else {
                state.pixels[y][x] = DEAD;
            }
        }
    }
}

static void game_of_life_update() {
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            int num_living_neighbours = 0;
            for (int ny = -1; ny < 2; ny++) {
                for (int nx = -1; nx < 2; nx++) {
                    if ((nx == 0) && (ny == 0)) {
                        continue;
                    }
                    if (state.pixels[(y+ny)&(IMAGE_HEIGHT-1)][(x+nx)&(IMAGE_WIDTH-1)] == LIVING) {
                        num_living_neighbours++;
                    }
                }
            }
            // any live cell...
            if (state.pixels[y][x] == LIVING) {
                if (num_living_neighbours < 2) {
                    // ... with fewer than 2 living neighbours dies, as if caused by underpopulation
                    state.pixels[y][x] = DEAD;
                } else if (num_living_neighbours > 3) {
                    // ... with more than 3 living neighbours dies, as if caused by overpopulation
                    state.pixels[y][x] = DEAD;
                }
            } else if (num_living_neighbours == 3) {
                // any dead cell with exactly 3 living neighbours becomes a live cell, as if by reproduction
                state.pixels[y][x] = LIVING;
            }
        }
    }
    if (state.update_count++ > 240) {
        game_of_life_init();
        state.update_count = 0;
    }
}
