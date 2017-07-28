//------------------------------------------------------------------------------
//  offscreen-glfw.c
//  Simple offscreen rendering.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_USE_GLCORE33
#include "sokol_gfx.h"

const int WIDTH = 640;
const int HEIGHT = 480;

typedef struct {
    hmm_mat4 mvp;
} params_t;

int main() {

    /* create window and GL context via GLFW */
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Offscreen GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit(w);

    /* setup sokol_gfx */
    sg_desc desc; 
    sg_init_desc(&desc);
    sg_setup(&desc);
    assert(sg_isvalid());

    /* a render target image */
    sg_image_desc img_desc;
    sg_init_image_desc(&img_desc);
    img_desc.render_target = true;
    img_desc.width = img_desc.height = 512;
    img_desc.color_format = SG_PIXELFORMAT_RGBA8;
    img_desc.depth_format = SG_PIXELFORMAT_DEPTH;
    img_desc.min_filter = img_desc.mag_filter = SG_FILTER_LINEAR;
    img_desc.wrap_u = img_desc.wrap_v = SG_WRAP_REPEAT;
    /* if supported, use MSAA for offscreen rendering */
    if (sg_query_feature(SG_FEATURE_MSAA_RENDER_TARGETS)) {
        img_desc.sample_count = 4;
    }
    sg_id img = sg_make_image(&img_desc);

    /* an offscreen pass rendering to that image */
    sg_pass_desc pass_desc;
    sg_init_pass_desc(&pass_desc);
    pass_desc.color_attachments[0].image = img;
    pass_desc.depth_stencil_attachment.image = img;
    sg_id offscreen_pass = sg_make_pass(&pass_desc);

    /* pass action for offscreen pass, clearing to black */ 
    sg_pass_action offscreen_pass_action;
    sg_init_pass_action(&offscreen_pass_action);
    offscreen_pass_action.color[0][0] = 0.0f;
    offscreen_pass_action.color[0][1] = 0.0f;
    offscreen_pass_action.color[0][2] = 0.0f;

    /* pass action for default pass, clearing to blue-ish */
    sg_pass_action default_pass_action;
    sg_init_pass_action(&default_pass_action);
    default_pass_action.color[0][0] = 0.0f;
    default_pass_action.color[0][1] = 0.25f;
    default_pass_action.color[0][2] = 1.0f;

    /* cube vertex buffer with positions, colors and tex coords */
    float vertices[] = {
        /* pos                  color                       uvs */
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
    sg_buffer_desc vbuf_desc;
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = sizeof(vertices);
    vbuf_desc.data_ptr = vertices;
    vbuf_desc.data_size = sizeof(vertices);
    sg_id vbuf = sg_make_buffer(&vbuf_desc);

    /* an index buffer for the cube */
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
    sg_id ibuf = sg_make_buffer(&ibuf_desc);

    /* shader for the non-textured cube, rendered in the offscreen pass */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "mvp", offsetof(params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    shd_desc.vs.source = 
        "#version 330\n"
        "uniform mat4 mvp;\n"
        "in vec4 position;\n"
        "in vec4 color0;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  gl_Position = mvp * position;\n"
        "  color = color0;\n"
        "}\n";
    shd_desc.fs.source =
        "#version 330\n"
        "in vec4 color;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "  frag_color = color;\n"
        "}\n";
    sg_id offscreen_shd = sg_make_shader(&shd_desc);

    /* ...and a second shader for rendering a textured cube in the default pass */
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "mvp", offsetof(params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    sg_init_named_image(&shd_desc, SG_SHADERSTAGE_FS, "tex", SG_IMAGETYPE_2D);
    shd_desc.vs.source = 
        "#version 330\n"
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
        "}\n";
    shd_desc.fs.source =
        "#version 330\n"
        "uniform sampler2D tex;\n"
        "in vec4 color;\n"
        "in vec2 uv;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "  frag_color = texture(tex, uv) + color * 0.5;\n"
        "}\n";
    sg_id default_shd = sg_make_shader(&shd_desc);

    /* pipeline object for offscreen rendering, don't need texcoords here */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 36);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = offscreen_shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.rast.cull_face_enabled = true;
    sg_id offscreen_pip = sg_make_pipeline(&pip_desc);

    /* and another pipeline object for the default pass */
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 36);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    sg_init_named_vertex_attr(&pip_desc, 0, "texcoord0", 28, SG_VERTEXFORMAT_FLOAT2);
    pip_desc.shader = default_shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.rast.cull_face_enabled = true;
    sg_id default_pip = sg_make_pipeline(&pip_desc);

    /* the draw state for offscreen rendering with all the required resources */
    sg_draw_state offscreen_ds;
    sg_init_draw_state(&offscreen_ds);
    offscreen_ds.pipeline = offscreen_pip;
    offscreen_ds.vertex_buffers[0] = vbuf;
    offscreen_ds.index_buffer = ibuf;

    /* and the draw state for the default pass where a textured cube will
       rendered, note how the render-target image is used as texture here */
    sg_draw_state default_ds;
    sg_init_draw_state(&default_ds);
    default_ds.pipeline = default_pip;
    default_ds.vertex_buffers[0] = vbuf;
    default_ds.index_buffer = ibuf;
    default_ds.fs_images[0] = img;

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* everything ready, on to the draw loop! */
    params_t vs_params = { };
    float rx = 0.0f, ry = 0.0f;
    while (!glfwWindowShouldClose(w)) {

        /* prepare the uniform block with the model-view-projection matrix,
           we just use the same matrix for the offscreen- and default-pass */
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 model = HMM_MultiplyMat4(
            HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f)),
            HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

        /* offscreen pass, this renders a rotating, untextured cube to the
           offscreen render target */
        sg_begin_pass(offscreen_pass, &offscreen_pass_action);
        sg_apply_draw_state(&offscreen_ds);
        sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();

        /* and the default pass, this renders a textured cube, using the 
           offscreen render target as texture image */
        sg_begin_default_pass(&default_pass_action, WIDTH, HEIGHT);
        sg_apply_draw_state(&default_ds);
        sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    sg_destroy_pipeline(default_pip);
    sg_destroy_pipeline(offscreen_pip);
    sg_destroy_shader(default_shd);
    sg_destroy_shader(offscreen_shd);
    sg_destroy_buffer(ibuf);
    sg_destroy_buffer(vbuf);
    sg_destroy_pass(offscreen_pass);
    sg_destroy_image(img);
    sg_shutdown();
    glfwTerminate();
}
