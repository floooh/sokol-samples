//------------------------------------------------------------------------------
//  mrt-emsc.c
//  Multiple-render-target sample for HTML5, this requires WebGL2.
//------------------------------------------------------------------------------
#include <stddef.h>     /* offsetof */
#include <GLES3/gl3.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "emsc.h"

static struct {
    float rx, ry;
    struct {
        sg_pass_desc pass_desc;
        sg_pass pass;
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sg_pipeline fsq_pip;
        sg_bindings fsq_bind;
        sg_pipeline dbg_pip;
        sg_bindings dbg_bind;
    } display;
} state;

typedef struct {
    float x, y, z;
    float b;
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} offscreen_params_t;

typedef struct {
    hmm_vec2 offset;
} params_t;

static void draw();
static void draw_webgl_fallback();

int main() {
    /* setup WebGL context */
    emsc_init("#canvas", EMSC_TRY_WEBGL2|EMSC_ANTIALIAS);

    /* setup sokol_gfx */
    sg_desc desc = {
        .context.gl.force_gles2 = emsc_webgl_fallback()
    };
    sg_setup(&desc);
    assert(sg_isvalid());

    /* not much useful things to do in this demo if WebGL2 is not supported,
       so just drop out and later render a dark red screen */
    if (desc.context.gl.force_gles2) {
        state.display.pass_action = (sg_pass_action){
            .colors[0] = { .action=SG_ACTION_CLEAR, .value={0.5f, 0.0f, 0.0f, 1.0f} }
        };
        emscripten_set_main_loop(draw_webgl_fallback, 0, 1);
        return 0;
    }

    /* a render pass with 3 color attachment images, and a depth attachment image */
    const int offscreen_sample_count = sg_query_features().msaa_render_targets ? 4 : 1;
    sg_image_desc color_img_desc = {
        .render_target = true,
        .width = emsc_width(),
        .height = emsc_height(),
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = offscreen_sample_count
    };
    sg_image_desc depth_img_desc = color_img_desc;
    depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    state.offscreen.pass_desc = (sg_pass_desc){
        .color_attachments = {
            [0].image = sg_make_image(&color_img_desc),
            [1].image = sg_make_image(&color_img_desc),
            [2].image = sg_make_image(&color_img_desc)
        },
        .depth_stencil_attachment.image = sg_make_image(&depth_img_desc)
    };
    state.offscreen.pass = sg_make_pass(&state.offscreen.pass_desc);

    /* a matching pass action with clear colors */
    state.offscreen.pass_action = (sg_pass_action){
        .colors = {
            [0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.0f, 0.0f, 1.0f } },
            [1] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.25f, 0.0f, 1.0f } },
            [2] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.25f, 1.0f } }
        }
    };

    /* cube vertex buffer */
    vertex_t cube_vertices[] = {
        /* pos + brightness */
        { -1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f,  1.0f, -1.0f,   1.0f },
        { -1.0f,  1.0f, -1.0f,   1.0f },

        { -1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f,  1.0f,  1.0f,   0.8f },
        { -1.0f,  1.0f,  1.0f,   0.8f },

        { -1.0f, -1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f,  1.0f,   0.6f },
        { -1.0f, -1.0f,  1.0f,   0.6f },

        { 1.0f, -1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f,  1.0f,    0.4f },
        { 1.0f, -1.0f,  1.0f,    0.4f },

        { -1.0f, -1.0f, -1.0f,   0.5f },
        { -1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f, -1.0f,   0.5f },

        { -1.0f,  1.0f, -1.0f,   0.7f },
        { -1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f, -1.0f,   0.7f },
    };
    sg_buffer cube_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices)
    });

    /* index buffer for the cube */
    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer cube_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(cube_indices)
    });

    /* resource bindings for offscreen rendering */
    state.offscreen.bind = (sg_bindings){
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    /* a shader to render the cube into offscreen MRT render targets */
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].name = "position",
            [1].name = "bright0"
        },
        .vs.uniform_blocks[0] = {
            .size = sizeof(offscreen_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .vs.source =
            "#version 300 es\n"
            "uniform mat4 mvp;\n"
            "in vec4 position;\n"
            "in float bright0;\n"
            "out float bright;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  bright = bright0;\n"
            "}\n",
        .fs.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in float bright;\n"
            "layout(location=0) out vec4 frag_color_0;\n"
            "layout(location=1) out vec4 frag_color_1;\n"
            "layout(location=2) out vec4 frag_color_2;\n"
            "void main() {\n"
            "  frag_color_0 = vec4(bright, 0.0, 0.0, 1.0);\n"
            "  frag_color_1 = vec4(0.0, bright, 0.0, 1.0);\n"
            "  frag_color_2 = vec4(0.0, 0.0, bright, 1.0);\n"
            "}\n"
    });

    /* pipeline object for the offscreen-rendered cube */
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [0] = { .offset=offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset=offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .color_count = 3,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = offscreen_sample_count
    });

    /* a vertex buffer to render a fullscreen rectangle */
    /* -> FIXME: we should allow bufferless rendering */
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_buf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices)
    });

    /* resource bindings to render the fullscreen quad */
    state.display.fsq_bind = (sg_bindings){
        .vertex_buffers[0] = quad_buf,
        .fs_images = {
            [0] = state.offscreen.pass_desc.color_attachments[0].image,
            [1] = state.offscreen.pass_desc.color_attachments[1].image,
            [2] = state.offscreen.pass_desc.color_attachments[2].image,
        }
    };

    /* a shader to render a fullscreen rectangle, which 'composes'
       the 3 offscreen render target images onto the screen */
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .attrs[0].name = "pos",
        .vs.uniform_blocks[0] = {
            .size = sizeof(params_t),
            .uniforms = {
                [0] = { .name="offset", .type=SG_UNIFORMTYPE_FLOAT2}
            }
        },
        .fs.images = {
            [0] = { .name="tex0", .image_type=SG_IMAGETYPE_2D },
            [1] = { .name="tex1", .image_type=SG_IMAGETYPE_2D },
            [2] = { .name="tex2", .image_type=SG_IMAGETYPE_2D }
        },
        .vs.source =
            "#version 300 es\n"
            "uniform vec2 offset;"
            "in vec2 pos;\n"
            "out vec2 uv0;\n"
            "out vec2 uv1;\n"
            "out vec2 uv2;\n"
            "void main() {\n"
            "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
            "  uv0 = pos + vec2(offset.x, 0.0);\n"
            "  uv1 = pos + vec2(0.0, offset.y);\n"
            "  uv2 = pos;\n"
            "}\n",
        .fs.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform sampler2D tex0;\n"
            "uniform sampler2D tex1;\n"
            "uniform sampler2D tex2;\n"
            "in vec2 uv0;\n"
            "in vec2 uv1;\n"
            "in vec2 uv2;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  vec3 c0 = texture(tex0, uv0).xyz;\n"
            "  vec3 c1 = texture(tex1, uv1).xyz;\n"
            "  vec3 c2 = texture(tex2, uv2).xyz;\n"
            "  frag_color = vec4(c0 + c1 + c2, 1.0);\n"
            "}\n"
    });

    /* the pipeline object for the fullscreen rectangle */
    state.display.fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    });

    /* and prepare a pipeline and bindings to render a debug visualizations
       of the offscreen render target contents
    */
    state.display.dbg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(&(sg_shader_desc){
            .attrs[0].name = "pos",
            .vs.source =
                "#version 300 es\n"
                "uniform vec2 offset;"
                "in vec2 pos;\n"
                "out vec2 uv;\n"
                "void main() {\n"
                "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
                "  uv = pos;\n"
                "}\n",
            .fs.images[0] = { .name="tex", .image_type=SG_IMAGETYPE_2D },
            .fs.source =
                "#version 300 es\n"
                "precision mediump float;\n"
                "uniform sampler2D tex;\n"
                "in vec2 uv;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = vec4(texture(tex,uv).xyz, 1.0);\n"
                "}\n"
        })
    });
    state.display.dbg_bind = (sg_bindings) {
        .vertex_buffers[0] = quad_buf,
        /* images will be filled right before rendering */
    };

    /* default pass action, no clear needed, since whole screen is overwritten */
    state.display.pass_action = (sg_pass_action){
        .colors = {
            [0].action = SG_ACTION_DONTCARE,
            [1].action = SG_ACTION_DONTCARE,
            [2].action = SG_ACTION_DONTCARE
        },
        .depth.action = SG_ACTION_DONTCARE,
        .stencil.action = SG_ACTION_DONTCARE
    };

    /* hand off control to browser-loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

void draw() {
    /* compute shader params */
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)emsc_width()/(float)emsc_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    const offscreen_params_t offscreen_params = {
        .mvp = HMM_MultiplyMat4(view_proj, model)
    };
    const params_t params = {
        .offset = HMM_Vec2(HMM_SinF(state.rx*0.01f)*0.1f, HMM_SinF(state.ry*0.01f)*0.1f)
    };

    /* render cube into MRT offscreen render targets */
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    /* render fullscreen quad with the 'composed image', plus 3 small
        debug-views with the content of the offscreen render targets */
    sg_begin_default_pass(&state.display.pass_action, emsc_width(), emsc_height());
    sg_apply_pipeline(state.display.fsq_pip);
    sg_apply_bindings(&state.display.fsq_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(params));
    sg_draw(0, 4, 1);
    sg_apply_pipeline(state.display.dbg_pip);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        state.display.dbg_bind.fs_images[0] = state.offscreen.pass_desc.color_attachments[i].image;
        sg_apply_bindings(&state.display.dbg_bind);
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
}

/* this is used as draw callback if webgl2 is not supported */
void draw_webgl_fallback() {
    float g = state.display.pass_action.colors[0].value.g + 0.01f;
    if (g > 0.5f) g = 0.0f;
    state.display.pass_action.colors[0].value.g = g;
    sg_begin_default_pass(&state.display.pass_action, emsc_width(), emsc_height());
    sg_end_pass();
    sg_commit();
}
