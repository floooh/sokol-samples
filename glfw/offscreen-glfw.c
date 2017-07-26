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
    img_desc.min_filter = img_desc.mag_filter = SG_FILTER_NEAREST;
    img_desc.wrap_u = img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sg_id img = sg_make_image(&img_desc);

    /* create an offscreen render pass */
    sg_pass_desc pass_desc;
    sg_init_pass_desc(&pass_desc);
    pass_desc.color_attachments[0].image = img;
    pass_desc.depth_stencil_attachment.image = img;
    sg_id pass = sg_make_pass(&pass_desc);

    /* cube vertex buffer with positions and tex coords */
    float vertices[] = {
        -1.0, -1.0, -1.0,   0.0, 0.0, 
         1.0, -1.0, -1.0,   1.0, 0.0,
         1.0,  1.0, -1.0,   1.0, 1.0,
        -1.0,  1.0, -1.0,   0.0, 1.0,

        -1.0, -1.0,  1.0,   0.0, 0.0,
         1.0, -1.0,  1.0,   1.0, 0.0,
         1.0,  1.0,  1.0,   1.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.0,
        -1.0,  1.0, -1.0,   1.0, 0.0,
        -1.0,  1.0,  1.0,   1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 1.0,

        1.0, -1.0, -1.0,   0.0, 0.0,
        1.0,  1.0, -1.0,   1.0, 0.0,
        1.0,  1.0,  1.0,   1.0, 1.0,
        1.0, -1.0,  1.0,   0.0, 1.0,

        -1.0, -1.0, -1.0,  0.0, 0.0,
        -1.0, -1.0,  1.0,  1.0, 0.0,
         1.0, -1.0,  1.0,  1.0, 1.0,
         1.0, -1.0, -1.0,  0.0, 1.0,

        -1.0,  1.0, -1.0,  0.0, 0.0,
        -1.0,  1.0,  1.0,  1.0, 0.0,
         1.0,  1.0,  1.0,  1.0, 1.0,
         1.0,  1.0, -1.0,  0.0, 1.0,
    };
    sg_buffer_desc vbuf_desc;
    sg_init_buffer_desc(&vbuf_desc);
    vbuf_desc.size = sizeof(vertices);
    vbuf_desc.data_ptr = vertices;
    vbuf_desc.data_size = sizeof(vertices);
    sg_id vbuf = sg_make_buffer(&vbuf_desc);

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
    sg_id ibuf = sg_make_buffer(&ibuf_desc);

    /* pass actions for offscreen- and default-pass */
    sg_pass_action offscreen_pass_action;
    sg_init_pass_action(&offscreen_pass_action);
    offscreen_pass_action.color[0][0] = 1.0f;
    offscreen_pass_action.color[0][1] = 0.5f;
    offscreen_pass_action.color[0][2] = 0.0f;
    sg_pass_action default_pass_action;
    sg_init_pass_action(&default_pass_action);
    default_pass_action.color[0][0] = 0.0f;
    default_pass_action.color[0][1] = 0.5f;
    default_pass_action.color[0][2] = 1.0f;

    /* the draw loop */
    while (!glfwWindowShouldClose(w)) {
        /* offscreen pass */
        sg_begin_pass(pass, &offscreen_pass_action);
        sg_end_pass();

        /* default pass */
        sg_begin_default_pass(&default_pass_action, WIDTH, HEIGHT);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    sg_destroy_buffer(ibuf);
    sg_destroy_buffer(vbuf);
    sg_destroy_pass(pass);
    sg_destroy_image(img);
    sg_shutdown();
    glfwTerminate();
}
