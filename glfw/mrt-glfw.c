//------------------------------------------------------------------------------
//  mrt-glfw.c
//  Multiple-render-target sample.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"

#define NUM_MRTS (3)
#define OFFSCREEN_SAMPLE_COUNT (4)

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

int main() {
    // create GLFW window and initialize GL
    glfw_init(&(glfw_desc_t){ .title = "mrt-glfw.c", .width = 800, .height = 600 });
    const int width = glfw_width();
    const int height = glfw_height();

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func,
    });
    assert(sg_isvalid());

    // color-, resolve- and depth-stencil attachment images, and texture views on the resolve images
    sg_image color_img[NUM_MRTS];
    sg_image resolve_img[NUM_MRTS];
    sg_view resolve_texview[NUM_MRTS];
    for (int i = 0; i < NUM_MRTS; i++) {
        color_img[i] = sg_make_image(&(sg_image_desc){
            .usage.color_attachment = true,
            .width = width,
            .height = height,
            .sample_count = OFFSCREEN_SAMPLE_COUNT,
        });
        resolve_img[i] = sg_make_image(&(sg_image_desc){
            .usage.resolve_attachment = true,
            .width = width,
            .height = height,
            .sample_count = 1,
        });
        resolve_texview[i] = sg_make_view(&(sg_view_desc){
            .texture.image = resolve_img[i],
        });
    }
    sg_image depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = width,
        .height = height,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
    });

    // a pass struct with attachment views and pass action
    const sg_pass offscreen_pass = (sg_pass){
        .attachments = {
            .colors = {
                [0] = sg_make_view(&(sg_view_desc){ .color_attachment.image = color_img[0] }),
                [1] = sg_make_view(&(sg_view_desc){ .color_attachment.image = color_img[1] }),
                [2] = sg_make_view(&(sg_view_desc){ .color_attachment.image = color_img[2] }),
            },
            .resolves = {
                [0] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = resolve_img[0] }),
                [1] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = resolve_img[1] }),
                [2] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = resolve_img[2] }),
            },
            .depth_stencil = sg_make_view(&(sg_view_desc){ .depth_stencil_attachment.image = depth_img }),
        },
        .action = {
            .colors = {
                [0] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.25f, 0.0f, 0.0f, 1.0f }
                },
                [1] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.0f, 0.25f, 0.0f, 1.0f }
                },
                [2] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.0f, 0.0f, 0.25f, 1.0f }
                }
            }
        },
    };

    // cube vertex buffer
    vertex_t cube_vertices[] = {
        // pos + brightness
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

    // index buffer for the cube
    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer cube_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices)
    });

    // a shader to render the cube into offscreen MRT render targets
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 410\n"
            "uniform mat4 mvp;\n"
            "layout(location=0) in vec4 position;\n"
            "layout(location=1) in float bright0;\n"
            "out float bright;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  bright = bright0;\n"
            "}\n",
        .fragment_func.source =
            "#version 410\n"
            "in float bright;\n"
            "layout(location=0) out vec4 frag_color_0;\n"
            "layout(location=1) out vec4 frag_color_1;\n"
            "layout(location=2) out vec4 frag_color_2;\n"
            "void main() {\n"
            "  frag_color_0 = vec4(bright, 0.0, 0.0, 1.0);\n"
            "  frag_color_1 = vec4(0.0, bright, 0.0, 1.0);\n"
            "  frag_color_2 = vec4(0.0, 0.0, bright, 1.0);\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(offscreen_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 }
            }
        }
    });

    // resource bindings for offscreen rendering
    sg_bindings offscreen_bind = {
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    // pipeline object for the offscreen-rendered cube
    sg_pipeline offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                /* offsets are not strictly needed here because the vertex layout
                   is gapless, but this demonstrates that offsetof() can be used
                */
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
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // a vertex buffer to render a fullscreen rectangle
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices)
    });

    // a sampler with linear filtering and clamp-to-edge
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    // resource bindings to render the fullscreen quad
    sg_bindings fsq_bind = {
        .vertex_buffers[0] = quad_vbuf,
        .views = {
            [0] = resolve_texview[0],
            [1] = resolve_texview[1],
            [2] = resolve_texview[2],
        },
        .samplers[0] = smp,
    };

    /* a shader to render a fullscreen rectangle, which 'composes'
       the 3 offscreen render target images onto the screen */
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 410\n"
            "uniform vec2 offset;"
            "layout(location=0) in vec2 pos;\n"
            "out vec2 uv0;\n"
            "out vec2 uv1;\n"
            "out vec2 uv2;\n"
            "void main() {\n"
            "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
            "  uv0 = pos + vec2(offset.x, 0.0);\n"
            "  uv1 = pos + vec2(0.0, offset.y);\n"
            "  uv2 = pos;\n"
            "}\n",
        .fragment_func.source =
            "#version 410\n"
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
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "offset", .type = SG_UNIFORMTYPE_FLOAT2 }
            }
        },
        .views = {
            [0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
            [1].texture.stage = SG_SHADERSTAGE_FRAGMENT,
            [2].texture.stage = SG_SHADERSTAGE_FRAGMENT,
        },
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .texture_sampler_pairs = {
            [0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex0", .view_slot = 0, .sampler_slot = 0 },
            [1] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex1", .view_slot = 1, .sampler_slot = 0 },
            [2] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex2", .view_slot = 2, .sampler_slot = 0 },
        },
    });

    // the pipeline object for the fullscreen rectangle
    sg_pipeline fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    });

    // pipeline and resource bindings to render a debug visualizations
    // of the offscreen render target contents
    sg_pipeline dbg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(&(sg_shader_desc){
            .vertex_func.source =
                "#version 410\n"
                "layout(location=0) in vec2 pos;\n"
                "out vec2 uv;\n"
                "void main() {\n"
                "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
                "  uv = pos;\n"
                "}\n",
            .fragment_func.source =
                "#version 330\n"
                "uniform sampler2D tex;\n"
                "in vec2 uv;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = vec4(texture(tex,uv).xyz, 1.0);\n"
                "}\n",
            .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
            .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
            .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex", .view_slot = 0, .sampler_slot = 0 },
        })
    });
    sg_bindings dbg_bind = {
        .vertex_buffers[0] = quad_vbuf,
        .samplers[0] = smp,
        // images will be filled right before rendering
    };

    // default pass action, no clear needed, since whole screen is overwritten
    sg_pass_action default_pass_action = {
        .colors[0].load_action = SG_LOADACTION_DONTCARE,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    offscreen_params_t offscreen_params;
    params_t params;
    float rx = 0.0f, ry = 0.0f;
    while (!glfwWindowShouldClose(glfw_window())) {
        // view-projection matrix for the offscreen-rendered cube
        hmm_mat4 proj = HMM_Perspective(60.0f, (float)glfw_width()/(float)glfw_height(), 0.01f, 10.0f);
        hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

        rx += 1.0f; ry += 2.0f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        offscreen_params.mvp = HMM_MultiplyMat4(view_proj, model);
        params.offset = HMM_Vec2(HMM_SinF(rx*0.01f)*0.1f, HMM_SinF(ry*0.01f)*0.1f);

        // render cube into MRT offscreen render targets
        sg_begin_pass(&offscreen_pass);
        sg_apply_pipeline(offscreen_pip);
        sg_apply_bindings(&offscreen_bind);
        sg_apply_uniforms(0, &SG_RANGE(offscreen_params));
        sg_draw(0, 36, 1);
        sg_end_pass();

        // render fullscreen quad with the 'composed image', plus 3 small
        // debug-views with the content of the offscreen render targets
        sg_begin_pass(&(sg_pass){
            .action = default_pass_action,
            .swapchain = glfw_swapchain(),
        });
        sg_apply_pipeline(fsq_pip);
        sg_apply_bindings(&fsq_bind);
        sg_apply_uniforms(0, &SG_RANGE(params));
        sg_draw(0, 4, 1);
        sg_apply_pipeline(dbg_pip);
        for (int i = 0; i < 3; i++) {
            sg_apply_viewport(i*100, 0, 100, 100, false);
            dbg_bind.views[0] = resolve_texview[i];
            sg_apply_bindings(&dbg_bind);
            sg_draw(0, 4, 1);
        }
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }
    sg_shutdown();
    glfwTerminate();
    return 0;
}
