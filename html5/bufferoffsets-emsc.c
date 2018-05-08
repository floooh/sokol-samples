//------------------------------------------------------------------------------
//  bufferoffsets-emsc.c
//  Render separate geometries in vertex- and index-buffers with
//  buffer offsets.
//------------------------------------------------------------------------------
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"
#include "emsc.h"

sg_draw_state draw_state;
sg_pass_action pass_action = {
    .colors = {
        [0] = { .action=SG_ACTION_CLEAR, .val = { 0.5f, 0.5f, 1.0f, 1.0f } }
    }
};

typedef struct {
    float x, y;
    float r, g, b;
} vertex_t;

void draw();

int main() {
    /* setup WebGL context */
    emsc_init("#canvas", EMSC_NONE);

    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());

    /* a 2D triangle and quad in 1 vertex buffer and 1 index buffer */
    vertex_t vertices[7] = {
        /* triangle */
        {  0.0f,   0.55f,  1.0f, 0.0f, 0.0f },
        {  0.25f,  0.05f,  0.0f, 1.0f, 0.0f },
        { -0.25f,  0.05f,  0.0f, 0.0f, 1.0f },
        /* quad */
        { -0.25f, -0.05f,  0.0f, 0.0f, 1.0f },
        {  0.25f, -0.05f,  0.0f, 1.0f, 0.0f },
        {  0.25f, -0.55f,  1.0f, 0.0f, 0.0f },
        { -0.25f, -0.55f,  1.0f, 1.0f, 0.0f }        
    };
    uint16_t indices[9] = {
        0, 1, 2,
        0, 1, 2, 0, 2, 3
    };
    draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });
    draw_state.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* create a shader to render 2D colored shapes */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "attribute vec2 pos;"
            "attribute vec3 color0;"
            "varying vec4 color;"
            "void main() {"
            "  gl_Position = vec4(pos, 0.5, 1.0);\n"
            "  color = vec4(color0, 1.0);\n"
            "}\n",
        .fs.source =
            "precision mediump float;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "  gl_FragColor = color;\n"
            "}\n"
    });

    /* a pipeline state object, default states are fine */
    draw_state.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0] = { .name="pos", .format=SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .name="color0", .format=SG_VERTEXFORMAT_FLOAT3 }
            }
        }
    });
    
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

void draw() {
    sg_begin_default_pass(&pass_action, emsc_width(), emsc_height());
    /* render triangle */
    draw_state.vertex_buffer_offsets[0] = 0;
    draw_state.index_buffer_offset = 0;
    sg_apply_draw_state(&draw_state);
    sg_draw(0, 3, 1);
    /* render quad */
    draw_state.vertex_buffer_offsets[0] = 3 * sizeof(vertex_t);
    draw_state.index_buffer_offset = 3 * sizeof(uint16_t);
    sg_apply_draw_state(&draw_state);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}