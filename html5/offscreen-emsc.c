//------------------------------------------------------------------------------
//  offscreen-emsc.c
//------------------------------------------------------------------------------
#include <stddef.h>     /* offsetof */
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"

static struct{
    float rx, ry;
    struct {
        sg_pass_action pass_action;
        sg_attachments attachments;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;

    } display;
} state = {
    // offscreen: clear to black
    .offscreen.pass_action.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    // display: clear to blue-ish
    .display.pass_action.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.25f, 1.0f, 1.0f } },
};

typedef struct {
    hmm_mat4 mvp;
} params_t;

static EM_BOOL draw(double time, void* userdata);

int main() {
    // setup WebGL context
    emsc_init("#canvas", EMSC_ANTIALIAS);

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = emsc_environment(),
        .logger.func = slog_func
    });
    assert(sg_isvalid());

    // create one color- and one depth-rendertarget image
    const int offscreen_sample_count = 1;
    sg_image_desc img_desc = {
        .usage.render_attachment = true,
        .width = 512,
        .height = 512,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = offscreen_sample_count
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    sg_image depth_img = sg_make_image(&img_desc);

    // an offscreen render pass into those images
    state.offscreen.attachments = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = color_img,
        .depth_stencil.image = depth_img
    });

    // a sampler for using the render target as texture
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    // cube vertex buffer with positions, colors and tex coords
    float vertices[] = {
        // pos                  color                       uvs
        -1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,

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
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // an index buffer for the cube
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // shader for the non-textured cube, rendered in the offscreen pass
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc) {
        .vertex_func.source =
            "#version 300 es\n"
            "uniform mat4 mvp;\n"
            "in vec4 position;\n"
            "in vec4 color0;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  color = color0;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n",
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "color0"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 }
            }
        },
    });

    // ...and a second shader for rendering a textured cube in the default pass
    sg_shader default_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 300 es\n"
            "uniform mat4 mvp;\n"
            "in vec4 position;\n"
            "in vec4 color0;\n"
            "in vec2 texcoord0;\n"
            "out vec4 color;\n"
            "out vec2 uv;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  color = color0;\n"
            "  uv = texcoord0;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;"
            "uniform sampler2D tex;\n"
            "in vec4 color;\n"
            "in vec2 uv;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = texture(tex, uv) + color * 0.5;\n"
            "}\n",
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "color0",
            [2].glsl_name = "texcoord0"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 }
            }
        },
        .images[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .image_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex", .image_slot = 0, .sampler_slot = 0 },
    });

    // pipeline object for offscreen rendering, don't need texcoords here
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // need to provide stride, because the buffer's texcoord is skipped
            .buffers[0].stride = 36,
            // but don't need to provide attr offsets, because pos and color are continuous
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = offscreen_sample_count
    });

    // and another pipeline object for the default pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // don't need to provide buffer stride or attr offsets, no gaps here
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4,
                [2].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = default_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK
    });

    // the resource bindings for offscreen rendering
    state.offscreen.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    // and the resource bindings for the default pass where a textured cube will
    // rendered, note how the color rendertarget image is used as texture here
    state.display.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .images[0] = color_img,
        .samplers[0] = smp,
    };

    // hand off control to browser loop
    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    // prepare the uniform block with the model-view-projection matrix,
    // we just use the same matrix for the offscreen- and default-pass
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)emsc_width()/(float)emsc_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 model = HMM_MultiplyMat4(
        HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f)),
        HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));
    const params_t vs_params = {
        .mvp = HMM_MultiplyMat4(view_proj, model)
    };

    // offscreen pass, this renders a rotating, untextured cube to the
    // offscreen render target
    sg_begin_pass(&(sg_pass){
        .action = state.offscreen.pass_action,
        .attachments = state.offscreen.attachments,
    });
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // and the default pass, this renders a textured cube, using the
    // offscreen render target as texture image
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = emsc_swapchain(),
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}
