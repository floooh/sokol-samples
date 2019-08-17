//------------------------------------------------------------------------------
//  mrt-glfw.c
//  Multiple-render-target sample.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"

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
    const int WIDTH = 640;
    const int HEIGHT = 480;

    /* create GLFW window and initialize GL */
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol MRT GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit();

    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());

    /* a render pass with 3 color attachment images, and a depth attachment image */
    const int offscreen_sample_count = sg_query_features().msaa_render_targets ? 4:1;
    sg_image_desc color_img_desc = {
        .render_target = true,
        .width = WIDTH,
        .height = HEIGHT,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = offscreen_sample_count
    };
    sg_image_desc depth_img_desc = color_img_desc;
    depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    sg_pass_desc offscreen_pass_desc = {
        .color_attachments = {
            [0].image = sg_make_image(&color_img_desc),
            [1].image = sg_make_image(&color_img_desc),
            [2].image = sg_make_image(&color_img_desc)
        },
        .depth_stencil_attachment.image = sg_make_image(&depth_img_desc)
    };
    sg_pass offscreen_pass = sg_make_pass(&offscreen_pass_desc);

    /* a matching pass action with clear colors */
    sg_pass_action offscreen_pass_action = {
        .colors = {
            [0] = { .action = SG_ACTION_CLEAR, .val = { 0.25f, 0.0f, 0.0f, 1.0f } },
            [1] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.25f, 0.0f, 1.0f } },
            [2] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.25f, 1.0f } }
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
        .size = sizeof(cube_vertices),
        .content = cube_vertices,
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
        .size = sizeof(cube_indices),
        .content = cube_indices,
    });

    /* a shader to render the cube into offscreen MRT render targets */
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(offscreen_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .vs.source =
            "#version 330\n"
            "uniform mat4 mvp;\n"
            "layout(location=0) in vec4 position;\n"
            "layout(location=1) in float bright0;\n"
            "out float bright;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  bright = bright0;\n"
            "}\n",
        .fs.source =
            "#version 330\n"
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

    /* resource bindings for offscreen rendering */
    sg_bindings offscreen_bind = {
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    /* pipeline object for the offscreen-rendered cube */
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
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .blend = {
            .color_attachment_count = 3,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = offscreen_sample_count
        }
    });

    /* a vertex buffer to render a fullscreen rectangle */
    /* -> FIXME: we should allow bufferless rendering */
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(quad_vertices),
        .content = quad_vertices,
    });

    /* resource bindings to render the fullscreen quad */
    sg_bindings fsq_bind = {
        .vertex_buffers[0] = quad_vbuf,
        .fs_images = {
            [0] = offscreen_pass_desc.color_attachments[0].image,
            [1] = offscreen_pass_desc.color_attachments[1].image,
            [2] = offscreen_pass_desc.color_attachments[2].image,
        }
    };

    /* a shader to render a fullscreen rectangle, which 'composes'
       the 3 offscreen render target images onto the screen */
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(params_t),
            .uniforms = {
                [0] = { .name="offset", .type=SG_UNIFORMTYPE_FLOAT2}
            }
        },
        .fs.images = {
            [0] = { .name="tex0", .type=SG_IMAGETYPE_2D },
            [1] = { .name="tex1", .type=SG_IMAGETYPE_2D },
            [2] = { .name="tex2", .type=SG_IMAGETYPE_2D }
        },
        .vs.source =
            "#version 330\n"
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
        .fs.source =
            "#version 330\n"
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
    sg_pipeline fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    });

    /* pipeline and resource bindings to render a debug visualizations
       of the offscreen render target contents
    */
    sg_pipeline dbg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(&(sg_shader_desc){
            .vs.source =
                "#version 330\n"
                "layout(location=0) in vec2 pos;\n"
                "out vec2 uv;\n"
                "void main() {\n"
                "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
                "  uv = pos;\n"
                "}\n",
            .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
            .fs.source =
                "#version 330\n"
                "uniform sampler2D tex;\n"
                "in vec2 uv;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = vec4(texture(tex,uv).xyz, 1.0);\n"
                "}\n"
        })
    });
    sg_bindings dbg_bind = {
        .vertex_buffers[0] = quad_vbuf,
        /* images will be filled right before rendering */
    };

    /* default pass action, no clear needed, since whole screen is overwritten */
    sg_pass_action default_pass_action = {
        .colors = { 
            [0].action = SG_ACTION_DONTCARE,
            [1].action = SG_ACTION_DONTCARE,
            [2].action = SG_ACTION_DONTCARE
        },
        .depth.action = SG_ACTION_DONTCARE,
        .stencil.action = SG_ACTION_DONTCARE
    };

    /* view-projection matrix for the offscreen-rendered cube */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    
    offscreen_params_t offscreen_params;
    params_t params;
    float rx = 0.0f, ry = 0.0f;
    while (!glfwWindowShouldClose(w)) {
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        offscreen_params.mvp = HMM_MultiplyMat4(view_proj, model);
        params.offset = HMM_Vec2(HMM_SinF(rx*0.01f)*0.1f, HMM_SinF(ry*0.01f)*0.1f);

        /* render cube into MRT offscreen render targets */
        sg_begin_pass(offscreen_pass, &offscreen_pass_action);
        sg_apply_pipeline(offscreen_pip);
        sg_apply_bindings(&offscreen_bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &offscreen_params, sizeof(offscreen_params));
        sg_draw(0, 36, 1);
        sg_end_pass();

        /* render fullscreen quad with the 'composed image', plus 3 small
           debug-views with the content of the offscreen render targets */
        int cur_width, cur_height;
        glfwGetFramebufferSize(w, &cur_width, &cur_height);
        sg_begin_default_pass(&default_pass_action, cur_width, cur_height);
        sg_apply_pipeline(fsq_pip);
        sg_apply_bindings(&fsq_bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &params, sizeof(params));
        sg_draw(0, 4, 1);
        sg_apply_pipeline(dbg_pip);
        for (int i = 0; i < 3; i++) {
            sg_apply_viewport(i*100, 0, 100, 100, false);
            dbg_bind.fs_images[0] = offscreen_pass_desc.color_attachments[i].image;
            sg_apply_bindings(&dbg_bind);
            sg_draw(0, 4, 1);
        }
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }
    sg_shutdown();
    glfwTerminate();
    return 0;
}
