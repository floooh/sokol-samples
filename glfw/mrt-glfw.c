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
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol MRT GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit(w);

    /* setup sokol_gfx */
    sg_desc desc;
    sg_init_desc(&desc);
    sg_setup(&desc);
    assert(sg_isvalid());

    /* a render pass with 3 color attachment images, and a depth attachment image */
    sg_pass_desc offscreen_pass_desc;
    sg_init_pass_desc(&offscreen_pass_desc);
    sg_image_desc img_desc;
    sg_init_image_desc(&img_desc);
    img_desc.render_target = true;
    img_desc.width = WIDTH;
    img_desc.height = HEIGHT;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.min_filter = img_desc.mag_filter = SG_FILTER_LINEAR;
    img_desc.wrap_u = img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    if (sg_query_feature(SG_FEATURE_MSAA_RENDER_TARGETS)) {
        img_desc.sample_count = 4;
    }
    for (int i = 0; i < 3; i++) {
        offscreen_pass_desc.color_attachments[i].image = sg_make_image(&img_desc);
    }
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    offscreen_pass_desc.depth_stencil_attachment.image = sg_make_image(&img_desc);
    sg_pass offscreen_pass = sg_make_pass(&offscreen_pass_desc);

    /* a matching pass action with clear colors */
    sg_pass_action offscreen_pass_action;
    sg_init_pass_action(&offscreen_pass_action);
    sg_init_clear_color(&offscreen_pass_action, 0, 0.25f, 0.0f, 0.0f, 1.0f);
    sg_init_clear_color(&offscreen_pass_action, 1, 0.0f, 0.25f, 0.0f, 1.0f);
    sg_init_clear_color(&offscreen_pass_action, 2, 0.0f, 0.0f, 0.25f, 1.0f);

    /* draw state for offscreen rendering */
    sg_draw_state offscreen_ds;
    sg_init_draw_state(&offscreen_ds);

    /* cube vertex buffer */
    vertex_t vertices[] = {
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
    sg_buffer_desc vbuf_desc;
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = sizeof(vertices);
    vbuf_desc.data_ptr = vertices;
    vbuf_desc.data_size = sizeof(vertices);
    offscreen_ds.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

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
    offscreen_ds.index_buffer = sg_make_buffer(&ibuf_desc);

    /* a shader to render the cube into offscreen MRT render targets */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(offscreen_params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "mvp", offsetof(offscreen_params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    shd_desc.vs.source =
        "#version 330\n"
        "uniform mat4 mvp;\n"
        "in vec4 position;\n"
        "in float bright0;\n"
        "out float bright;\n"
        "void main() {\n"
        "  gl_Position = mvp * position;\n"
        "  bright = bright0;\n"
        "}\n";
    shd_desc.fs.source =
        "#version 330\n"
        "in float bright;\n"
        "layout(location=0) out vec4 frag_color_0;\n"
        "layout(location=1) out vec4 frag_color_1;\n"
        "layout(location=2) out vec4 frag_color_2;\n"
        "void main() {\n"
        "  frag_color_0 = vec4(bright, 0.0, 0.0, 1.0);\n"
        "  frag_color_1 = vec4(0.0, bright, 0.0, 1.0);\n"
        "  frag_color_2 = vec4(0.0, 0.0, bright, 1.0);\n"
        "}\n";

    /* pipeline object for the offscreen-rendered cube */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, sizeof(vertex_t));
    sg_init_named_vertex_attr(&pip_desc, 0, "position", offsetof(vertex_t, x), SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "bright0", offsetof(vertex_t, b), SG_VERTEXFORMAT_FLOAT);
    pip_desc.shader = sg_make_shader(&shd_desc);
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.rast.cull_mode = SG_CULLMODE_BACK;
    offscreen_ds.pipeline = sg_make_pipeline(&pip_desc);

    /* draw state to render a fullscreen quad */
    sg_draw_state fsq_ds;
    sg_init_draw_state(&fsq_ds);

    /* a vertex buffer to render a fullscreen rectangle */
    /* -> FIXME: we should allow bufferless rendering */
    float fsq_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = vbuf_desc.data_size = sizeof(fsq_vertices);
    vbuf_desc.data_ptr = fsq_vertices;
    fsq_ds.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

    /* a shader to render a fullscreen rectangle, which 'composes'
       the 3 offscreen render target images onto the screen */
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "offset", offsetof(params_t, offset), SG_UNIFORMTYPE_FLOAT2, 1);
    sg_init_named_image(&shd_desc, SG_SHADERSTAGE_FS, "tex0", SG_IMAGETYPE_2D);
    sg_init_named_image(&shd_desc, SG_SHADERSTAGE_FS, "tex1", SG_IMAGETYPE_2D);
    sg_init_named_image(&shd_desc, SG_SHADERSTAGE_FS, "tex2", SG_IMAGETYPE_2D);
    shd_desc.vs.source =
        "#version 330\n"
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
        "}\n";
    shd_desc.fs.source =
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
        "}\n";
    
    /* the pipeline object for the fullscreen rectangle */
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 8);
    sg_init_named_vertex_attr(&pip_desc, 0, "pos", 0, SG_VERTEXFORMAT_FLOAT2);
    pip_desc.shader = sg_make_shader(&shd_desc);
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
    fsq_ds.pipeline = sg_make_pipeline(&pip_desc);

    /* ...and the render target images as textures */
    for (int i = 0; i < 3; i++) {
        fsq_ds.fs_images[i] = offscreen_pass_desc.color_attachments[i].image; 
    }
    
    /* default pass action, no clear needed, since whole screen is overwritten */
    sg_pass_action default_pass_action;
    sg_init_pass_action(&default_pass_action);
    default_pass_action.actions = SG_PASSACTION_DONTCARE_ALL;

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
        sg_apply_draw_state(&offscreen_ds);
        sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &offscreen_params, sizeof(offscreen_params));
        sg_draw(0, 36, 1);
        sg_end_pass();

        /* render fullscreen quad with the 'composed image'
            FIXME: also want smaller 'debug view' with separate render target content,
            to test sg_apply_viewport */
        sg_begin_default_pass(&default_pass_action, WIDTH, HEIGHT);
        sg_apply_draw_state(&fsq_ds);
        sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &params, sizeof(params));
        sg_draw(0, 4, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }
    sg_shutdown();
    glfwTerminate();
    return 0;
}