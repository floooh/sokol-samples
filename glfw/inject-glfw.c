//------------------------------------------------------------------------------
//  inject-glwf.c
//  Demonstrate usage of injected native GL buffer and image resources.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "glfw_glue.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

// constants (VS doesn't like "const int" for array size)
enum {
    IMG_WIDTH = 32,
    IMG_HEIGHT = 32,
};

uint32_t pixels[IMG_WIDTH * IMG_HEIGHT];

typedef struct {
    mat44_t mvp;
} vs_params_t;

static mat44_t compute_mvp(float rx, float ry, int width, int height) {
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)width/(float)height, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, view_proj);
}

int main() {

    /* create GLFW window and initialize GL */
    glfw_init(&(glfw_desc_t){ .title = "inject-glfw.c", .width = 640, .height = 480, .sample_count = 4 });

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = glfw_environment(),
        .logger.func = slog_func
    });

    // create a native GL vertex and index buffer
    float vertices[] = {
        // pos                  uvs
        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

         1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,    0.0f, 1.0f,

        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 1.0f
    };
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    GLuint gl_vbuf, gl_ibuf;
    glGenBuffers(1, &gl_vbuf);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glGenBuffers(1, &gl_ibuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_ibuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /*
       create sokol vertex and index buffer from externally created GL buffer
       objects, it's important to call sg_reset_state_cache() after calling
       GL functions and before calling sokol functions again
    */
    sg_reset_state_cache();
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.immutable = true,
        .size = sizeof(vertices),
        .gl_buffers[0] = gl_vbuf
    });
    assert(sg_gl_query_buffer_info(vbuf).buf[0] == gl_vbuf);
    assert(sg_gl_query_buffer_info(vbuf).buf[1] == 0);
    assert(sg_gl_query_buffer_info(vbuf).active_slot == 0);
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .index_buffer = true,
            .immutable = true,
        },
        .size = sizeof(indices),
        .gl_buffers[0] = gl_ibuf
    });
    assert(sg_gl_query_buffer_info(ibuf).buf[0] == gl_ibuf);
    assert(sg_gl_query_buffer_info(ibuf).buf[1] == 0);
    assert(sg_gl_query_buffer_info(ibuf).active_slot == 0);

    /* create dynamically updated textures, in the GL backend
       dynamic textures are rotated through, so need to create
       SG_NUM_INFLIGHT_FRAMES GL textures
    */
    sg_image_desc img_desc = {
        .usage.stream_update = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        // testing gl_texture_target, not strictly needed in this case though:
        .gl_texture_target = GL_TEXTURE_2D
    };
    glGenTextures(SG_NUM_INFLIGHT_FRAMES, img_desc.gl_textures);
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        glBindTexture(GL_TEXTURE_2D, img_desc.gl_textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMG_WIDTH, IMG_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }
    sg_reset_state_cache();
    sg_image img = sg_make_image(&img_desc);
    assert(sg_gl_query_image_info(img).tex[0] == img_desc.gl_textures[0]);
    assert(sg_gl_query_image_info(img).tex[1] == img_desc.gl_textures[1]);
    assert(sg_gl_query_image_info(img).active_slot == 0);
    assert(sg_gl_query_image_info(img).tex_target == GL_TEXTURE_2D);

    // create a texture view on the image
    sg_view tex_view = sg_make_view(&(sg_view_desc){ .texture.image = img });

    // create a GL sampler object
    sg_sampler_desc smp_desc = {
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
    };
    glGenSamplers(1, &smp_desc.gl_sampler);
    glSamplerParameteri(smp_desc.gl_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(smp_desc.gl_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(smp_desc.gl_sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(smp_desc.gl_sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
    sg_reset_state_cache();
    sg_sampler smp = sg_make_sampler(&smp_desc);
    assert(sg_gl_query_sampler_info(smp).smp == smp_desc.gl_sampler);

    // define resource bindings
    sg_bindings bind = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .views[0] = tex_view,
        .samplers[0] = smp,
    };

    // create shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 410\n"
            "uniform mat4 mvp;\n"
            "layout(location=0) in vec4 position;\n"
            "layout(location=1) in vec2 texcoord0;\n"
            "out vec2 uv;"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  uv = texcoord0;\n"
            "}\n",
        .fragment_func.source =
            "#version 410\n"
            "uniform sampler2D tex;"
            "in vec2 uv;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = texture(tex, uv);\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "mvp", .type = SG_UNIFORMTYPE_MAT4 },
            }
        },
        .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex", .view_slot = 0, .sampler_slot = 0 },
    });
    assert(sg_gl_query_shader_info(shd).prog != 0);

    // create pipeline object
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK
    });

    // default pass action
    sg_pass_action pass_action = { 0 };

    float rx = 0.0f, ry = 0.0f;
    uint32_t counter = 0;
    while (!glfwWindowShouldClose(glfw_window())) {
        rx += 1.0f; ry += 2.0f;
        const vs_params_t vs_params = { .mvp = compute_mvp(rx, ry, glfw_width(), glfw_height()) };

        // update texture image with some generated pixel data
        for (int y = 0; y < IMG_WIDTH; y++) {
            for (int x = 0; x < IMG_HEIGHT; x++) {
                pixels[y * IMG_WIDTH + x] = 0xFF000000 |
                             (counter & 0xFF)<<16 |
                             ((counter*3) & 0xFF)<<8 |
                             ((counter*23) & 0xFF);
                counter++;
            }
        }
        counter++;
        sg_update_image(img, &(sg_image_data){ .mip_levels[0] = SG_RANGE(pixels) });

        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = glfw_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(glfw_window());
        glfwPollEvents();
    }

    sg_shutdown();

    // sokol_gfx doesn't destroy any externally created resource object
    glDeleteBuffers(1, &gl_vbuf); gl_vbuf = 0;
    glDeleteBuffers(1, &gl_ibuf); gl_ibuf = 0;
    glDeleteTextures(SG_NUM_INFLIGHT_FRAMES, img_desc.gl_textures);
    glDeleteSamplers(1, &smp_desc.gl_sampler);

    glfwTerminate();
}
