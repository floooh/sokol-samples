//------------------------------------------------------------------------------
//  blend-glfw.c
//  Test blend factor combinations.
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
    hmm_mat4 mvp;
} vs_params_t;

typedef struct {
    float tick;
} fs_params_t;

enum { NUM_BLEND_FACTORS = 15 };
sg_pipeline pips[NUM_BLEND_FACTORS][NUM_BLEND_FACTORS];

int main() {
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const int MSAA_SAMPLES = 4;

    /* create GLFW window and initialize GL */
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, MSAA_SAMPLES);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Blend GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit();

    /* setup sokol_gfx (need to increase pipeline pool size) */
    sg_desc desc = {
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1
    };
    sg_setup(&desc);

    /* a quad vertex buffer */
    float vertices[] = {
        /* pos               color */
        -1.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.5f,
        +1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.5f,
        -1.0f, +1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.5f,
        +1.0f, +1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.5f
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* a shader for the fullscreen background quad */
    sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
        .vs.source =
            "#version 330\n"
            "layout(location=0) in vec2 position;\n"
            "void main() {\n"
            "  gl_Position = vec4(position, 0.5, 1.0);\n"
            "}\n",
        .fs = {
            .uniform_blocks[0] = {
                .size = sizeof(fs_params_t),
                .uniforms = {
                    [0] = { .name="tick", .type=SG_UNIFORMTYPE_FLOAT }
                }
            },
            .source =
                "#version 330\n"
                "uniform float tick;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);\n"
                "  frag_color = vec4(vec3(xy.x*xy.y), 1.0);\n"
                "}\n"
        }
    });

    /* a pipeline state object for rendering the background quad */
    sg_pipeline bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = bg_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    /* a shader for the blended quads */
    sg_shader quad_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
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
            "}"
    });

    /* one pipeline object per blend-factor combination */
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = quad_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend = {
            .enabled = true,
            .blend_color = { 1.0f, 0.0f, 0.0f, 1.0f },
        },
    };
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++) {
            const sg_blend_factor src_blend = (sg_blend_factor) (src+1);
            const sg_blend_factor dst_blend = (sg_blend_factor) (dst+1);
            pip_desc.blend.src_factor_rgb = src_blend;
            pip_desc.blend.dst_factor_rgb = dst_blend;
            pip_desc.blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
            pip_desc.blend.dst_factor_alpha = SG_BLENDFACTOR_ZERO;
            pips[src][dst] = sg_make_pipeline(&pip_desc);
            assert(pips[src][dst].id != SG_INVALID_ID);
        }
    }

    /* a pass action which does not clear, since the entire screen is overwritten anyway */
    sg_pass_action pass_action = {
        .colors[0].action = SG_ACTION_DONTCARE ,
        .depth.action = SG_ACTION_DONTCARE,
        .stencil.action = SG_ACTION_DONTCARE
    };

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(90.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 100.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 25.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    sg_bindings bind = {
        .vertex_buffers[0] = vbuf
    };
    vs_params_t vs_params;
    fs_params_t fs_params;
    float r = 0.0f;
    fs_params.tick = 0.0f;
    while (!glfwWindowShouldClose(w)) {
        int cur_width, cur_height;
        glfwGetFramebufferSize(w, &cur_width, &cur_height);
        sg_begin_default_pass(&pass_action, cur_width, cur_height);

        /* draw a background quad */
        sg_apply_pipeline(bg_pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_params, sizeof(fs_params));
        sg_draw(0, 4, 1);

        /* draw the blended quads */
        float r0 = r;
        for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
            for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
                /* compute new model-view-proj matrix */
                hmm_mat4 rm = HMM_Rotate(r0, HMM_Vec3(0.0f, 1.0f, 0.0f));
                const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
                const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
                hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(HMM_Vec3(x, y, 0.0f)), rm);
                vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

                sg_apply_pipeline(pips[src][dst]);
                sg_apply_bindings(&bind);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
                sg_draw(0, 4, 1);
            }
        }
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
        r += 0.6f;
        fs_params.tick += 1.0f;
    }
    sg_shutdown();
    glfwTerminate();
    return 0;
}
