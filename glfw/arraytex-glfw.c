//------------------------------------------------------------------------------
//  arraytex-glfw.c
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

typedef struct {
    hmm_mat4 mvp;
    hmm_vec2 offset0;
    hmm_vec2 offset1;
    hmm_vec2 offset2;
} params_t;

enum {
    IMG_LAYERS = 3,
    IMG_WIDTH = 16,
    IMG_HEIGHT = 16
};

int main() {
    // create GLFW window and initialize GL
    glfw_init("arraytex-glfw.c", 800, 600, 1);
    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });
    assert(sg_isvalid());

    // a 16x16 array texture with 3 layers and a checkerboard pattern
    static uint32_t pixels[IMG_LAYERS][IMG_HEIGHT][IMG_WIDTH];
    for (int layer=0, even_odd=0; layer<IMG_LAYERS; layer++) {
        for (int y = 0; y < IMG_HEIGHT; y++, even_odd++) {
            for (int x = 0; x < IMG_WIDTH; x++, even_odd++) {
                if (even_odd & 1) {
                    switch (layer) {
                        case 0: pixels[layer][y][x] = 0x000000FF; break;
                        case 1: pixels[layer][y][x] = 0x0000FF00; break;
                        case 2: pixels[layer][y][x] = 0x00FF0000; break;
                    }
                }
                else {
                    pixels[layer][y][x] = 0;
                }
            }
        }
    }
    sg_image img = sg_make_image(&(sg_image_desc) {
        .type = SG_IMAGETYPE_ARRAY,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .num_slices = IMG_LAYERS,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.subimage[0][0] = SG_RANGE(pixels)
    });

    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    // cube vertex buffer
    float vertices[] = {
        // pos                  uvs
        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

         1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 1.0f
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
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    /* resource bindings */
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs = {
            .images[0] = img,
            .samplers[0] = smp,
        }
    };

    /* shader to sample from array texture */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0] = {
                .size = sizeof(params_t),
                .uniforms = {
                    [0] = { .name="mvp",     .type=SG_UNIFORMTYPE_MAT4 },
                    [1] = { .name="offset0", .type=SG_UNIFORMTYPE_FLOAT2 },
                    [2] = { .name="offset1", .type=SG_UNIFORMTYPE_FLOAT2 },
                    [3] = { .name="offset2", .type=SG_UNIFORMTYPE_FLOAT2 }
                }
            },
            .source =
                "#version 330\n"
                "uniform mat4 mvp;\n"
                "uniform vec2 offset0;\n"
                "uniform vec2 offset1;\n"
                "uniform vec2 offset2;\n"
                "layout(location=0) in vec4 position;\n"
                "layout(location=1) in vec2 texcoord0;\n"
                "out vec3 uv0;\n"
                "out vec3 uv1;\n"
                "out vec3 uv2;\n"
                "void main() {\n"
                "  gl_Position = mvp * position;\n"
                "  uv0 = vec3(texcoord0 + offset0, 0.0);\n"
                "  uv1 = vec3(texcoord0 + offset1, 1.0);\n"
                "  uv2 = vec3(texcoord0 + offset2, 2.0);\n"
                "}\n",
        },
        .fs = {
            .images[0] = { .used = true, .image_type = SG_IMAGETYPE_ARRAY },
            .samplers[0].used = true,
            .image_sampler_pairs[0] = { .used = true, .glsl_name = "tex", .image_slot = 0, .sampler_slot = 0 },
            .source =
                "#version 330\n"
                "uniform sampler2DArray tex;\n"
                "in vec3 uv0;\n"
                "in vec3 uv1;\n"
                "in vec3 uv2;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  vec4 c0 = texture(tex, uv0);\n"
                "  vec4 c1 = texture(tex, uv1);\n"
                "  vec4 c2 = texture(tex, uv2);\n"
                "  frag_color = vec4(c0.xyz + c1.xyz + c2.xyz, 1.0);\n"
                "}\n",
        }
    });

    // create pipeline object
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
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

    // default pass action
    sg_pass_action pass_action = {
        .colors[0] = { .load_action=SG_LOADACTION_CLEAR, .clear_value={0.0f, 0.0f, 0.0f, 1.0f} }
    };

    params_t vs_params;
    float rx = 0.0f, ry = 0.0f;
    int frame_index = 0;
    while (!glfwWindowShouldClose(glfw_window())) {
        // view-projection matrix
        hmm_mat4 proj = HMM_Perspective(60.0f, (float)glfw_width()/(float)glfw_height(), 0.01f, 10.0f);
        hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

        // rotated model matrix
        rx += 0.25f; ry += 0.5f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);

        // model-view-projection matrix for vertex shader
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
        // uv offsets
        float offset = (float)frame_index * 0.0001f;
        vs_params.offset0 = HMM_Vec2(-offset, offset);
        vs_params.offset1 = HMM_Vec2(offset, -offset);
        vs_params.offset2 = HMM_Vec2(0.0f, 0.0f);

        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
        frame_index++;
    }

    sg_shutdown();
    glfwTerminate();
}
