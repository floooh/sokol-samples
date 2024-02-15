//------------------------------------------------------------------------------
//  mipmap-glfw.c
//  Test mipmapping behaviour.
//  Top row: NEAREST_MIPMAP_NEAREST to LINEAR_MIPMAP_LINEAR
//  Bottom row: anistropy levels 2, 4, 8 and 16
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
} vs_params_t;

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

uint32_t mip_colors[9] = {
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

int main() {
    // create GLFW window and initialize GL
    glfw_init("mipmap-glfw.c", 800, 600, 4);

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });

    // a plane vertex buffer
    float vertices[] = {
        -1.0, -1.0, 0.0,  0.0, 0.0,
        +1.0, -1.0, 0.0,  1.0, 0.0,
        -1.0, +1.0, 0.0,  0.0, 1.0,
        +1.0, +1.0, 0.0,  1.0, 1.0,
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // initialize mipmap content, different colors and checkboard pattern
    sg_image_data img_data;
    uint32_t* ptr = pixels.mip0;
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

    // create a single image and 12 samplers
    sg_image img = sg_make_image(&(sg_image_desc){
        .width = 256,
        .height = 256,
        .num_mipmaps = 9,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data = img_data
    });

    // the first 4 samplers are just different min-filters
    sg_sampler smp[12];
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
            smp[smp_index++] = sg_make_sampler(&smp_desc);
        }
    }
    // the next 4 samplers use min_lod/max_lod
    smp_desc.min_lod = 2.0f;
    smp_desc.max_lod = 4.0f;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            smp_desc.min_filter = filters[i];
            smp_desc.mipmap_filter = mipmap_filters[j];
            smp[smp_index++] = sg_make_sampler(&smp_desc);
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
        smp[smp_index++] = sg_make_sampler(&smp_desc);
    }
    assert(smp_index == 12);

    // shader
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
                "layout(location=1) in vec2 texcoord0;\n"
                "out vec2 uv;\n"
                "void main() {\n"
                "  gl_Position = mvp * position;\n"
                "  uv = texcoord0;\n"
                "}\n"
        },
        .fs = {
            .images[0].used = true,
            .samplers[0].used = true,
            .image_sampler_pairs[0] = { .used = true, .glsl_name = "tex", .image_slot = 0, .sampler_slot = 0 },
            .source =
                "#version 330\n"
                "uniform sampler2D tex;"
                "in vec2 uv;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = texture(tex, uv);\n"
                "}\n"
        }
    });

    // pipeline state
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .fs.images[0] = img,
    };
    vs_params_t vs_params;
    float r = 0.0f;
    while (!glfwWindowShouldClose(glfw_window())) {
        // view-projection matrix
        hmm_mat4 proj = HMM_Perspective(90.0f, (float)glfw_width()/(float)glfw_height(), 0.01f, 10.0f);
        hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 5.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

        r += 0.1f;
        hmm_mat4 rm = HMM_Rotate(r, HMM_Vec3(1.0f, 0.0f, 0.0f));

        sg_begin_pass(&(sg_pass){ .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        for (int i = 0; i < 12; i++) {
            const float x = ((float)(i & 3) - 1.5f) * 2.0f;
            const float y = ((float)(i / 4) - 1.0f) * -2.0f;
            hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
            vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
            bind.fs.samplers[0] = smp[i];
            sg_apply_bindings(&bind);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
            sg_draw(0, 4, 1);
        }
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }
    sg_shutdown();
    glfwTerminate();
}
