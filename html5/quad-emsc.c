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
#define SOKOL_USE_GLES2
#include "sokol_gfx.h"

const int WIDTH = 640;
const int HEIGHT = 480;

sg_draw_state draw_state;
sg_pass_action pass_action;

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
    sg_desc desc;
    sg_init_desc(&desc);
    sg_setup(&desc);
    assert(sg_isvalid());
    
    /* clear the global draw state struct */
    sg_init_draw_state(&draw_state);
    
    /* create a vertex buffer */
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,        
    };
    sg_buffer_desc vbuf_desc;
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = sizeof(vertices);
    vbuf_desc.data_ptr = vertices;
    vbuf_desc.data_size = sizeof(vertices);
    draw_state.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

    /* create an index buffer */
    uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle        
    };
    sg_buffer_desc ibuf_desc;
    sg_init_buffer_desc(&ibuf_desc);
    ibuf_desc.size = sizeof(indices);
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.data_ptr = indices;
    ibuf_desc.data_size = sizeof(indices);
    draw_state.index_buffer = sg_make_buffer(&ibuf_desc);

    /* create a shader */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    shd_desc.vs.source = 
        "attribute vec4 position;\n"
        "attribute vec4 color0;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_Position = position;\n"
        "  color = color0;\n"
        "}\n";
    shd_desc.fs.source = 
        "precision mediump float;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_FragColor = color;\n"
        "}\n";
    sg_id shd_id = sg_make_shader(&shd_desc);
    
    /* create a pipeline object (default render state is fine) */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = shd_id;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    draw_state.pipeline = sg_make_pipeline(&pip_desc);

    /* default pass action (clear to grey) */
    sg_init_pass_action(&pass_action);

    /* hand off control to browser loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

/* draw one frame */
void draw() {
    sg_begin_pass(SG_DEFAULT_PASS, &pass_action, WIDTH, HEIGHT);
    sg_apply_draw_state(&draw_state);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}