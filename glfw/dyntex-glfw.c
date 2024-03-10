//------------------------------------------------------------------------------
//  dyntex-glfw.c
//  update texture per-frame with CPU generated data
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

// width/height must be 2^N
#define IMAGE_WIDTH (64)
#define IMAGE_HEIGHT (64)
#define LIVING (0xFFFFFFFF)
#define DEAD  (0xFF000000)
uint32_t pixels[IMAGE_WIDTH][IMAGE_HEIGHT];

void game_of_life_init();
void game_of_life_update();

int main() {
    // create GLFW window and initialize GL
    glfw_init(&(glfw_desc_t){ .title = "dyntex-glfw.c", .width = 640, .height = 480, .sample_count = 4 });

    // init sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });

    // a 128x128 image with streaming-update strategy
    sg_image img = sg_make_image(&(sg_image_desc){
        .width = IMAGE_WIDTH,
        .height = IMAGE_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
    });

    // a sampler object
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    // a cube vertex- and index-buffer
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
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    // define the resource bindings
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs = {
            .images[0] = img,
            .samplers[0] = smp,
        }
    };

    // a shader to render textured cube
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0] = {
                .size = sizeof(vs_params_t),
                .uniforms = {
                    [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
                }
            },
            .source =
                "#version 330\n"
                "uniform mat4 mvp;\n"
                "layout(location=0) in vec4 position;\n"
                "layout(location=1) in vec4 color0;\n"
                "layout(location=2) in vec2 texcoord0;\n"
                "out vec2 uv;"
                "out vec4 color;"
                "void main() {\n"
                "  gl_Position = mvp * position;\n"
                "  uv = texcoord0;\n"
                "  color = color0;\n"
                "}\n",
        },
        .fs = {
            .images[0].used = true,
            .samplers[0].used = true,
            .image_sampler_pairs[0] = { .used = true, .glsl_name = "tex", .image_slot = 0, .sampler_slot = 0 },
            .source =
                "#version 330\n"
                "uniform sampler2D tex;\n"
                "in vec4 color;\n"
                "in vec2 uv;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = texture(tex, uv) * color;\n"
                "}\n"
        }
    });

    // a pipeline-state-object for the textured cube
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
        .cull_mode = SG_CULLMODE_BACK
    });

    // default pass action (clear to grey)
    sg_pass_action pass_action = {0};

    // initial game-of-life seed state
    game_of_life_init();

    vs_params_t vs_params;
    float rx = 0.0f, ry = 0.0f;
    while (!glfwWindowShouldClose(glfw_window())) {
        // view-projection matrix
        hmm_mat4 proj = HMM_Perspective(60.0f, (float)glfw_width()/(float)glfw_height(), 0.01f, 10.0f);
        hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 4.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

        // model-view-projection matrix from rotated model matrix
        rx += 0.1f; ry += 0.2f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        // update game-of-life state
        game_of_life_update();

        // update the dynamic image
        sg_update_image(img, &(sg_image_data){
            .subimage[0][0] = {
                .ptr=pixels,
                .size=sizeof(pixels)
            }
        });

        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    sg_shutdown();
    glfwTerminate();
}

void game_of_life_init() {
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            if ((rand() & 255) > 230) {
                pixels[y][x] = LIVING;
            } else {
                pixels[y][x] = DEAD;
            }
        }
    }
}

void game_of_life_update() {
    static int update_count = 0;
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            int num_living_neighbours = 0;
            for (int ny = -1; ny < 2; ny++) {
                for (int nx = -1; nx < 2; nx++) {
                    if ((nx == 0) && (ny == 0)) {
                        continue;
                    }
                    if (pixels[(y+ny)&(IMAGE_HEIGHT-1)][(x+nx)&(IMAGE_WIDTH-1)] == LIVING) {
                        num_living_neighbours++;
                    }
                }
            }
            // any live cell...
            if (pixels[y][x] == LIVING) {
                if (num_living_neighbours < 2) {
                    // ... with fewer than 2 living neighbours dies, as if caused by underpopulation
                    pixels[y][x] = DEAD;
                } else if (num_living_neighbours > 3) {
                    // ... with more than 3 living neighbours dies, as if caused by overpopulation
                    pixels[y][x] = DEAD;
                }
            } else if (num_living_neighbours == 3) {
                // any dead cell with exactly 3 living neighbours becomes a live cell, as if by reproduction
                pixels[y][x] = LIVING;
            }
        }
    }
    if (update_count++ > 240) {
        game_of_life_init();
        update_count = 0;
    }
}
