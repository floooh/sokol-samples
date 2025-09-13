//------------------------------------------------------------------------------
//  inject-emsc.c
//  Demonstrate usage of injected native GL buffer and image resources.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "emsc.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

#define IMG_WIDTH (32)
#define IMG_HEIGHT (32)
static uint32_t pixels[IMG_WIDTH * IMG_HEIGHT];

static struct {
    sg_pipeline pip;
    sg_image img;
    sg_bindings bind;
    sg_pass_action pass_action;
    float rx, ry;
    uint32_t counter;
} state = {
    .pass_action.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
};

typedef struct {
    mat44_t mvp;
} vs_params_t;

static EM_BOOL draw(double time, void* userdata);

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
    // setup WebGL context
    emsc_init("#canvas", EMSC_ANTIALIAS);

    // setup sokol_gfx
    sg_setup(&(sg_desc){
        .environment = emsc_environment(),
        .logger.func = slog_func
    });

    // create a native GL vertex and index buffer
    float vertices[] = {
        /* pos                  uvs */
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
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage.immutable = true,
        .size = sizeof(vertices),
        .gl_buffers[0] = gl_vbuf
    });
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .index_buffer = true,
            .immutable = true,
        },
        .size = sizeof(indices),
        .gl_buffers[0] = gl_ibuf
    });

    /* create dynamically updated textures, in the GL backend,
       dynamic textures are rotated through, so need to create
       SG_NUM_INFLIGHT_FRAMES GL textures
    */
    sg_image_desc img_desc = {
        .usage.stream_update = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
    };
    glGenTextures(SG_NUM_INFLIGHT_FRAMES, img_desc.gl_textures);
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        glBindTexture(GL_TEXTURE_2D, img_desc.gl_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMG_WIDTH, IMG_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }
    sg_reset_state_cache();
    state.img = sg_make_image(&img_desc);
    state.bind.views[0] = sg_make_view(&(sg_view_desc){ .texture.image = state.img });

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
    state.bind.samplers[0] = sg_make_sampler(&smp_desc);

    // create shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "#version 300 es\n"
            "uniform mat4 mvp;\n"
            "in vec4 position;\n"
            "in vec2 texcoord0;\n"
            "out vec2 uv;"
            "void main() {\n"
            "  gl_Position = mvp * position;\n"
            "  uv = texcoord0;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform sampler2D tex;\n"
            "in vec2 uv;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = texture(tex, uv);\n"
            "}\n",
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "texcoord0"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            },
        },
        .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex", .view_slot = 0, .sampler_slot = 0 },
    });

    // create pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
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

    // hand off control to browser loop
    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    state.rx += 1.0f; state.ry += 2.0f;
    const vs_params_t vs_params = { .mvp = compute_mvp(state.rx, state.ry, emsc_width(), emsc_height()) };

    // update texture image with some generated pixel data
    for (int y = 0; y < IMG_WIDTH; y++) {
        for (int x = 0; x < IMG_HEIGHT; x++) {
            pixels[y * IMG_WIDTH + x] = 0xFF000000 |
                            (state.counter & 0xFF)<<16 |
                            ((state.counter*3) & 0xFF)<<8 |
                            ((state.counter*23) & 0xFF);
            state.counter++;
        }
    }
    state.counter++;
    sg_update_image(state.img, &(sg_image_data){ .mip_levels[0] = SG_RANGE(pixels) });

    // ...and draw
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = emsc_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}
