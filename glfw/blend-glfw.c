//------------------------------------------------------------------------------
//  blend-glfw.c
//  Test blend factor combinations.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

typedef struct {
    mat44_t mvp;
} vs_params_t;

typedef struct {
    float tick;
} fs_params_t;

enum { NUM_BLEND_FACTORS = 15 };
sg_pipeline pips[NUM_BLEND_FACTORS][NUM_BLEND_FACTORS];

int main() {
    // create GLFW window and initialize GL
    glfw_init(&(glfw_desc_t){ .title = "blend-glfw.c", .width = 800, .height = 600, .sample_count = 4 });
    // setup sokol_gfx (need to increase pipeline pool size)
    sg_desc desc = {
        .pipeline_pool_size = NUM_BLEND_FACTORS * NUM_BLEND_FACTORS + 1,
        .environment = glfw_environment(),
        .logger.func = slog_func,
    };
    sg_setup(&desc);

    // a quad vertex buffer
    float vertices[] = {
        // pos               color
        -1.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.5f,
        +1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.5f,
        -1.0f, +1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.5f,
        +1.0f, +1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.5f
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // a shader for the fullscreen background quad
    sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 410\n"
            "layout(location=0) in vec2 position;\n"
            "void main() {\n"
            "  gl_Position = vec4(position, 0.5, 1.0);\n"
            "}\n",
        .fragment_func.source =
            "#version 410\n"
            "uniform float tick;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);\n"
            "  frag_color = vec4(vec3(xy.x*xy.y), 1.0);\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(fs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "tick", .type = SG_UNIFORMTYPE_FLOAT },
            }
        }
    });

    // a pipeline state object for rendering the background quad
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

    // a shader for the blended quads
    sg_shader quad_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 410\n"
            "uniform mat4 mvp;\n"
            "layout(location=0) in vec4 position;\n"
            "layout(location=1) in vec4 color0;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  color = color0;\n"
            "}\n",
        .fragment_func.source =
            "#version 410\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 }
            }
        },
    });

    // one pipeline object per blend-factor combination
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = quad_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend_color = { 1.0f, 0.0f, 0.0f, 1.0f },
    };
    for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
        for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++) {
            pip_desc.colors[0].blend = (sg_blend_state) {
                .enabled = true,
                .src_factor_rgb = (sg_blend_factor) (src+1),
                .dst_factor_rgb = (sg_blend_factor) (dst+1),
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ZERO,
            };
            pips[src][dst] = sg_make_pipeline(&pip_desc);
            assert(pips[src][dst].id != SG_INVALID_ID);
        }
    }

    // a pass action which does not clear, since the entire screen is overwritten anyway
    sg_pass_action pass_action = {
        .colors[0].load_action = SG_LOADACTION_DONTCARE ,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    sg_bindings bind = {
        .vertex_buffers[0] = vbuf
    };
    fs_params_t fs_params;
    float r = 0.0f;
    fs_params.tick = 0.0f;
    while (!glfwWindowShouldClose(glfw_window())) {
        const mat44_t proj = mat44_perspective_fov_rh(vm_radians(90.0f), (float)glfw_width()/(float)glfw_height(), 0.01f, 100.0f);
        const mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, 20.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat44_t view_proj = vm_mul(view, proj);

        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });

        // draw a background quad
        sg_apply_pipeline(bg_pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(fs_params));
        sg_draw(0, 4, 1);

        // draw the blended quads
        float r0 = r;
        for (int src = 0; src < NUM_BLEND_FACTORS; src++) {
            for (int dst = 0; dst < NUM_BLEND_FACTORS; dst++, r0+=0.6f) {
                // compute new model-view-proj matrix
                const mat44_t rm = mat44_rotation_y(vm_radians(r0));
                const float x = ((float)(dst - NUM_BLEND_FACTORS/2)) * 3.0f;
                const float y = ((float)(src - NUM_BLEND_FACTORS/2)) * 2.2f;
                const mat44_t model = vm_mul(rm, mat44_translation(x, y, 0.0f));
                const vs_params_t vs_params = { .mvp = vm_mul(model, view_proj) };
                sg_apply_pipeline(pips[src][dst]);
                sg_apply_bindings(&bind);
                sg_apply_uniforms(0, &SG_RANGE(vs_params));
                sg_draw(0, 4, 1);
            }
        }
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
        r += 0.6f;
        fs_params.tick += 1.0f;
    }
    sg_shutdown();
    glfwTerminate();
    return 0;
}
