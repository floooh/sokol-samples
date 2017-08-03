//------------------------------------------------------------------------------
//  quad-emsc.c
//  Render from vertex- and index-buffer.
//------------------------------------------------------------------------------
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"

const int WIDTH = 640;
const int HEIGHT = 480;

sg_draw_state draw_state;

/* default pass action (clear to grey) */
sg_pass_action pass_action = { 0 };

void draw();

int main() {
    /* setup WebGL context */
    emscripten_set_canvas_size(WIDTH, HEIGHT);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    ctx = emscripten_webgl_create_context(0, &attrs);
    emscripten_webgl_make_context_current(ctx);

    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());
    
    /* a vertex buffer */
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,        
    };
    sg_buffer_desc vbuf_desc = {
        .size = sizeof(vertices),
        .data_ptr = vertices,
        .data_size = sizeof(vertices)
    };

    /* an index buffer */
    uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle        
    };
    sg_buffer_desc ibuf_desc = {
        .size = sizeof(indices),
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data_ptr = indices,
        .data_size = sizeof(indices)
    };

    /* create a shader */
    sg_shader shd_id = sg_make_shader(&(sg_shader_desc) {
        .vs.source =
            "attribute vec4 position;\n"
            "attribute vec4 color0;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "  gl_Position = position;\n"
            "  color = color0;\n"
            "}\n",
        .fs.source =
            "precision mediump float;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "  gl_FragColor = color;\n"
            "}\n"
    });
    
    /* a pipeline object (default render state is fine) */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 28);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = shd_id;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;

    /* setup draw state with resource bindings */
    draw_state = (sg_draw_state){
        .pipeline = sg_make_pipeline(&pip_desc),
        .vertex_buffers[0] = sg_make_buffer(&vbuf_desc),
        .index_buffer = sg_make_buffer(&ibuf_desc)
    };

    /* hand off control to browser loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

/* draw one frame */
void draw() {
    sg_begin_default_pass(&pass_action, WIDTH, HEIGHT);
    sg_apply_draw_state(&draw_state);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}