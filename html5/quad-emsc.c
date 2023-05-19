//------------------------------------------------------------------------------
//  quad-emsc.c
//  Render from vertex- and index-buffer.
//------------------------------------------------------------------------------
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"

static struct {
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
} state = {
    .pass_action.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
};

static EM_BOOL draw(double time, void* userdata);

int main() {
    /* setup WebGL context */
    emsc_init("#canvas", EMSC_NONE);

    /* setup sokol_gfx */
    sg_desc desc = { .logger.func = slog_func };
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
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    /* an index buffer */
    uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    /* create a shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc) {
        .attrs = {
            [0].name = "position",
            [1].name = "color0"
        },
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
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* test to provide attr offsets, but no buffer stride, this should compute the stride */
            .attrs = {
                /* vertex attrs can also be bound by location instead of name (but not in GLES2) */
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16
    });

    /* hand off control to browser loop */
    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

/* draw one frame */
static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    sg_begin_default_pass(&state.pass_action, emsc_width(), emsc_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}
