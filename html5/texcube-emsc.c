//------------------------------------------------------------------------------
//  texcube-emsc.c
//------------------------------------------------------------------------------
#include <stddef.h>     /* offsetof */
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_USE_GLES2
#include "sokol_gfx.h"

const int WIDTH = 640;
const int HEIGHT = 480;

sg_draw_state draw_state;
sg_pass_action pass_action;
float rx = 0.0f;
float ry = 0.0f;
hmm_mat4 view_proj;

typedef struct {
    hmm_mat4 mvp;
} params_t;

void draw();

int main() {
    /* setup WebGL context */
    emscripten_set_canvas_size(WIDTH, HEIGHT);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.antialias = true;
    ctx = emscripten_webgl_create_context(0, &attrs);
    emscripten_webgl_make_context_current(ctx);

    /* setup sokol_gfx */
    sg_desc desc;
    sg_init_desc(&desc);
    sg_setup(&desc);
    assert(sg_isvalid());
    
    sg_init_draw_state(&draw_state);
    sg_init_pass_action(&pass_action);
    
    /* cube vertex buffer */
    float vertices[] = {
        /* pos                  color                       uvs */
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
    sg_buffer_desc vbuf_desc;
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = sizeof(vertices);
    vbuf_desc.data_ptr = vertices;
    vbuf_desc.data_size = sizeof(vertices);
    draw_state.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

    /* create an index buffer for the cube */
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer_desc ibuf_desc;
    sg_init_buffer_desc(&ibuf_desc);
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.size = sizeof(indices);
    ibuf_desc.data_ptr = indices;
    ibuf_desc.data_size = sizeof(indices);
    draw_state.index_buffer = sg_make_buffer(&ibuf_desc);

    /* create a checkerboard texture */
    uint32_t pixels[4*4] = {
        0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000,
        0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
        0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000,
        0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    };
    const void* img_data_ptrs[] = { pixels };
    const int img_data_sizes[] = { sizeof(pixels) };
    sg_image_desc img_desc;
    sg_init_image_desc(&img_desc);
    img_desc.width = img_desc.height = 4;
    img_desc.color_format = SG_PIXELFORMAT_RGBA8;
    img_desc.min_filter = img_desc.mag_filter = SG_FILTER_LINEAR;
    img_desc.wrap_u = img_desc.wrap_v = SG_WRAP_REPEAT;
    img_desc.num_data_items = 1;
    img_desc.data_ptrs = img_data_ptrs;
    img_desc.data_sizes = img_data_sizes; 
    draw_state.fs_images[0] = sg_make_image(&img_desc);

    /* create shader */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "mvp", offsetof(params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    sg_init_named_image(&shd_desc, SG_SHADERSTAGE_FS, "tex", SG_IMAGETYPE_2D);
    shd_desc.vs.source = 
        "uniform mat4 mvp;\n"
        "attribute vec4 position;\n"
        "attribute vec4 color0;\n"
        "attribute vec2 texcoord0;\n"
        "varying vec4 color;\n"
        "varying vec2 uv;"
        "void main() {\n"
        "  gl_Position = mvp * position;\n"
        "  color = color0;\n"
        "  uv = texcoord0 * 5.0;\n"
        "}\n";
    shd_desc.fs.source =
        "precision mediump float;\n"
        "uniform sampler2D tex;\n"
        "varying vec4 color;\n"
        "varying vec2 uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(tex, uv) * color;\n"
        "}\n";
    sg_id shd = sg_make_shader(&shd_desc);

    /* create pipeline object */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 36);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    sg_init_named_vertex_attr(&pip_desc, 0, "texcoord0", 28, SG_VERTEXFORMAT_FLOAT2);
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.rast.cull_face_enabled = true;
    draw_state.pipeline = sg_make_pipeline(&pip_desc);
    
    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);
    
    /* hand off control to browser loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

void draw() {
    /* compute model-view-projection matrix for vertex shader */
    params_t vs_params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 model = HMM_MultiplyMat4(
        HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f)),
        HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    /* ...and draw */
    sg_begin_default_pass(&pass_action, WIDTH, HEIGHT);
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}
