//------------------------------------------------------------------------------
//  multiwindow-glfw.c
//  How to use sokol-gfx with multiple GLFW windows (or one way to do it at least).
//  All sokol-gfx rendering goes into different framebuffers in the main GL context
//  and will then be blitted into the default framebuffers of the per-window GL contexts.
//
//  NOTE: we're taking a little shortcut here and don't resize the rendering
//  render buffers when the window size changes, instead the window content
//  will be scaled during the blit operation.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "flextgl33/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_EXTERNAL_GL_LOADER
#include "sokol_gfx.h"
#include "sokol_log.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

static bool flext_initialized = false;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    const char* title;
    GLFWwindow* glfw_main_window;
    sg_color clear_color;
} window_desc_t;

typedef struct {
    int orig_width, orig_height;
    GLFWwindow* glfw;
    GLuint color_rb;        // a shared color render buffer
    GLuint depth_rb;        // a shared depth-stencil render buffer
    GLuint main_fb;         // a framebuffer on the main GL context
    GLuint win_fb;          // a framebuffer on the window's GL context
    sg_pass_action pass_action;
} window_t;

// vertex shader params
typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

// create a GLFW window and associated framebuffers
static window_t create_window(const window_desc_t* desc) {
    assert(desc);
    assert((desc->width > 0) && (desc->height > 0) && (desc->title));
    window_t win = {
        .orig_width = desc->width,
        .orig_height = desc->height,
        .pass_action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = desc->clear_color},
        }
    };

    // create GLFW window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, 0);
    win.glfw = glfwCreateWindow(desc->width, desc->height, desc->title, 0, desc->glfw_main_window);
    glfwSetWindowPos(win.glfw, desc->x, desc->y);
    glfwMakeContextCurrent(win.glfw);
    if (!flext_initialized) {
        flextInit();
        flext_initialized = true;
    }
    glfwSwapInterval(1);

    // create shared render buffers on main context
    if (desc->glfw_main_window) {
        glfwMakeContextCurrent(desc->glfw_main_window);
    }
    glGenRenderbuffers(1, &win.color_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, win.color_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, desc->width, desc->height);
    glGenRenderbuffers(1, &win.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, win.depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, desc->width, desc->height);

    // create a framebuffer on the main gl context, this is where sokol-rendering goes into
    glGenFramebuffers(1, &win.main_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, win.main_fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, win.color_rb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, win.depth_rb);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    // create a framebuffer on the window gl context which shares its renderbuffers with
    // the main-context framebuffer, this framebuffer will be the source for a blit-framebuffer operation
    glfwMakeContextCurrent(win.glfw);
    glGenFramebuffers(1, &win.win_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, win.win_fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, win.color_rb);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    // restore main gl context
    if (desc->glfw_main_window) {
        glfwMakeContextCurrent(desc->glfw_main_window);
    }
    return win;
}

static void close_window(window_t* win) {
    assert(win && win->glfw);
    glDeleteFramebuffers(1, &win->win_fb); win->win_fb = 0;
    glDeleteFramebuffers(1, &win->main_fb); win->main_fb = 0;
    glDeleteRenderbuffers(1, &win->color_rb); win->color_rb = 0;
    glDeleteRenderbuffers(1, &win->depth_rb); win->depth_rb = 0;
    glfwDestroyWindow(win->glfw); win->glfw = 0;
}

static sg_environment get_environment(void) {
    return (sg_environment){
        .defaults = {
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .sample_count = 1,
        },
    };
}

static sg_swapchain get_swapchain(const window_t* win) {
    assert(win && win->glfw);
    return (sg_swapchain){
        .width = win->orig_width,
        .height = win->orig_height,
        .sample_count = 1,
        .color_format = SG_PIXELFORMAT_RGBA8,
        .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .gl.framebuffer = win->main_fb,
    };
}

int main() {
    const int WIDTH = 512;
    const int HEIGHT = 384;
    glfwInit();

    // create 3 windows and associated GL resources
    window_t win[3];
    win[0] = create_window(&(window_desc_t){
        .x = 40,
        .y = 40,
        .width = WIDTH,
        .height = HEIGHT,
        .title = "Main Window",
        .clear_color = { 0.5f, 0.5f, 1.0f, 1.0f },
    });
    win[1] = create_window(&(window_desc_t){
        .x = 40 + WIDTH,
        .y = 40,
        .width = WIDTH,
        .height = HEIGHT,
        .title = "Window 1",
        .glfw_main_window = win[0].glfw,
        .clear_color = { 1.0f, 0.5f, 0.5f, 1.0f },
    });
    win[2] = create_window(&(window_desc_t){
        .x = 40 + WIDTH / 2,
        .y = 40 + HEIGHT,
        .width = WIDTH,
        .height = HEIGHT,
        .title = "Window 2",
        .glfw_main_window = win[0].glfw,
        .clear_color = { 0.5f, 1.0f, 0.5f, 1.0f },
    });

    // setup sokol-gfx on main GL context
    glfwMakeContextCurrent(win[0].glfw);
    sg_setup(&(sg_desc) {
        .environment = get_environment(),
        .logger.func = slog_func
    });

    // cube vertices and indices
    static const float vertices[] = {
        -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
        -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

        -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

        1.0, -1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
        1.0,  1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
        1.0,  1.0,  1.0,    1.0, 0.5, 0.0, 1.0,
        1.0, -1.0,  1.0,    1.0, 0.5, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

        -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
        -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
    };
    static const uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_bindings bindings = {
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(vertices)
        }),
        .index_buffer = sg_make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .data = SG_RANGE(indices)
        }),
    };
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 },
            }
        },
        .vs.source =
            "#version 330\n"
            "uniform mat4 mvp;\n"
            "layout(location=0) in vec4 position;\n"
            "layout(location=1) in vec4 color0;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  color = color0;\n"
            "}\n",
        .fs.source =
            "#version 330\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n"
    });
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    // just use the same view-proj matrix, ignoring current window aspect ratio
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // run until main window is closed
    vs_params_t vs_params = {0};
    float rx = 0.0f, ry = 0.0f;
    while (win[0].glfw) {
        glfwPollEvents();
        // rotated model matrix
        rx += 1.0f; ry += 2.0f;
        hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
        vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
        // make sure main GL context is current and that sokol's cached GL state doesn't get confused
        // for each open window...
        for (int i = 0; i < 3; i++) {
            if (win[i].glfw) {
                // render a sokol-pass into the main GL context framebuffer associated with the window
                sg_begin_pass(&(sg_pass){ .action = win[i].pass_action, .swapchain = get_swapchain(&win[i])});
                sg_apply_pipeline(pip);
                sg_apply_bindings(&bindings);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
                sg_draw(0, 36, 1);
                sg_end_pass();
            }
        }
        sg_commit();

        // switch to the window's local GL context, and blit into the window's default framebuffer
        for (int i = 0; i < 3; i++) {
            if (win[i].glfw) {
                int dst_width, dst_height;
                glfwGetFramebufferSize(win[i].glfw, &dst_width, &dst_height);
                glfwMakeContextCurrent(win[i].glfw);
                glScissor(0, 0, dst_width, dst_height);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, win[i].win_fb);
                glBlitFramebuffer(0, 0, WIDTH, HEIGHT, 0, 0, dst_width, dst_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                glfwSwapBuffers(win[i].glfw);
                if (glfwWindowShouldClose(win[i].glfw)) {
                    close_window(&win[i]);
                }
            }
        }
        // switch back to main context and flush the sokol-gfx state cache
        glfwMakeContextCurrent(win[0].glfw);
        sg_reset_state_cache();
    }
    sg_shutdown();
    glfwTerminate();
    return 0;
}
