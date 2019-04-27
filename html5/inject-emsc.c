//------------------------------------------------------------------------------
//  inject-emsc.c
//  Demonstrate usage of injected native GL buffer and image resources.
//------------------------------------------------------------------------------
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"
#include "emsc.h"

static const int IMG_WIDTH = 32;
static const int IMG_HEIGHT = 32;
static uint32_t pixels[IMG_WIDTH * IMG_HEIGHT];

static sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
};
static sg_pipeline pip;
static sg_bindings bind;
static float rx, ry;
static uint32_t counter;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static void draw();

int main() {
    /* setup WebGL context */
    emsc_init("#canvas", EMSC_ANTIALIAS);

    /* setup sokol_gfx */
    sg_setup(&(sg_desc){0});

    /* create a native GL vertex and index buffer */
    float vertices[] = {
        /* pos                  uvs */
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
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    GLuint gl_vbuf, gl_ibuf;
    glGenBuffers(1, &gl_vbuf);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glGenBuffers(1, &gl_ibuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_ibuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /*
       create sokol vertex and index buffer from externally created GL buffer
       objects, it's important to call sg_reset_state_cache() after calling
       GL functions and before calling sokol functions again
    */
    sg_reset_state_cache();
    bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_IMMUTABLE,
        .size = sizeof(vertices),
        .gl_buffers[0] = gl_vbuf
    });
    bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_IMMUTABLE,
        .size = sizeof(indices),
        .gl_buffers[0] = gl_ibuf
    });

    /* create dynamically updated textures, in the GL backend,
       dynamic textures are rotated through, so need to create
       SG_NUM_INFLIGHT_FRAMES GL textures
    */
    sg_image_desc img_desc = {
        .usage = SG_USAGE_STREAM,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
    };
    glGenTextures(SG_NUM_INFLIGHT_FRAMES, img_desc.gl_textures);
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        glBindTexture(GL_TEXTURE_2D, img_desc.gl_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMG_WIDTH, IMG_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }
    sg_reset_state_cache();
    bind.fs_images[0] = sg_make_image(&img_desc);

    /* create shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].name = "position",
            [1].name = "texcoord0"
        },
        .vs.uniform_blocks[0] = { 
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            },
        },
        .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
        .vs.source =
            "uniform mat4 mvp;\n"
            "attribute vec4 position;\n"
            "attribute vec2 texcoord0;\n"
            "varying vec2 uv;"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  uv = texcoord0;\n"
            "}\n",
        .fs.source =
            "precision mediump float;\n"
            "uniform sampler2D tex;\n"
            "varying vec2 uv;\n"
            "void main() {\n"
            "  gl_FragColor = texture2D(tex, uv);\n"
            "}\n"
    });

    /* create pipeline object */
    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer.cull_mode = SG_CULLMODE_BACK
    });

    /* hand off control to browser loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

void draw() {
    /* compute model-view-projection matrix for vertex shader */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)emsc_width()/(float)emsc_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 model = HMM_MultiplyMat4(
        HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f)),
        HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    /* update texture image with some generated pixel data */
    for (int y = 0; y < IMG_WIDTH; y++) {
        for (int x = 0; x < IMG_HEIGHT; x++) {
            pixels[y * IMG_WIDTH + x] = 0xFF000000 |
                            (counter & 0xFF)<<16 |
                            ((counter*3) & 0xFF)<<8 |
                            ((counter*23) & 0xFF);
            counter++;
        }
    }
    counter++;
    sg_update_image(bind.fs_images[0], &(sg_image_content){
        .subimage[0][0] = { .ptr = pixels, .size = sizeof(pixels) }
    });

    /* ...and draw */
    sg_begin_default_pass(&pass_action, emsc_width(), emsc_height());
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}
