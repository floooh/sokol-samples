//------------------------------------------------------------------------------
//  restart-sapp.c
//
//  Test whether 'restarting' works for various sokol headers (calling the
//  shutdown functions, followed by initialization and continuing). There
//  should be no crashes or memory leaks.
//
//  This sample links against a version of the sokol implementations library
//  which uses the sokol_memtrack.h utility header to track memory allocations
//  in the sokol headers.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_audio.h"
#include "sokol_fetch.h"
#include "sokol_gl.h"
#include "sokol_debugtext.h"
#include "sokol_memtrack.h"
#include "sokol_glue.h"
#include "stb/stb_image.h"

#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "restart-sapp.glsl.h"

static struct {
    float rx, ry;
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    uint8_t file_buffer[256 * 1024];
    smemtrack_info_t init_mem_info;
} state;

typedef struct {
    float x, y, z;
    int16_t u, v;
} vertex_t;

/* cube vertices and indices */
static const vertex_t cube_vertices[] = {
    { -1.0f, -1.0f, -1.0f,      0,     0 },
    {  1.0f, -1.0f, -1.0f,  32767,     0 },
    {  1.0f,  1.0f, -1.0f,  32767, 32767 },
    { -1.0f,  1.0f, -1.0f,      0, 32767 },

    { -1.0f, -1.0f,  1.0f,      0,     0 },
    {  1.0f, -1.0f,  1.0f,  32767,     0 },
    {  1.0f,  1.0f,  1.0f,  32767, 32767 },
    { -1.0f,  1.0f,  1.0f,      0, 32767 },

    { -1.0f, -1.0f, -1.0f,      0,     0 },
    { -1.0f,  1.0f, -1.0f,  32767,     0 },
    { -1.0f,  1.0f,  1.0f,  32767, 32767 },
    { -1.0f, -1.0f,  1.0f,      0, 32767 },

    {  1.0f, -1.0f, -1.0f,      0,     0 },
    {  1.0f,  1.0f, -1.0f,  32767,     0 },
    {  1.0f,  1.0f,  1.0f,  32767, 32767 },
    {  1.0f, -1.0f,  1.0f,      0, 32767 },

    { -1.0f, -1.0f, -1.0f,      0,     0 },
    { -1.0f, -1.0f,  1.0f,  32767,     0 },
    {  1.0f, -1.0f,  1.0f,  32767, 32767 },
    {  1.0f, -1.0f, -1.0f,      0, 32767 },

    { -1.0f,  1.0f, -1.0f,      0,     0 },
    { -1.0f,  1.0f,  1.0f,  32767,     0 },
    {  1.0f,  1.0f,  1.0f,  32767, 32767 },
    {  1.0f,  1.0f, -1.0f,      0, 32767 },
};

static const uint16_t cube_indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

static void fetch_callback(const sfetch_response_t*);

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static void init(void) {

    // capture initial allocation values for leak detection
    state.init_mem_info = smemtrack_info();

    // setup sokol libraries...
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    sfetch_setup(&(sfetch_desc_t){ 0 });
    sgl_setup(&(sgl_desc_t){ 0 });
    sdtx_setup(&(sdtx_desc_t){
        .context_pool_size = 1,
        .printf_buf_size = 256,
        .fonts[0] = sdtx_font_cpc(),
        .context = {
            .char_buf_size = 256,
        }
    });

    // setup rendering resources
    state.bind.fs_images[0] = sg_alloc_image();

    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(cube_vertices),
        .content = cube_vertices,
        .label = "cube-vertices"
    });

    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(cube_indices),
        .content = cube_indices,
        .label = "cube-indices"
    });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(restart_shader_desc()),
        .layout = {
            .attrs = {
                [ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_texcoord0].format = SG_VERTEXFORMAT_SHORT2N
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
        },
        .label = "cube-pipeline"
    });

    // start loading the texture
    sfetch_send(&(sfetch_request_t){
        .path = "baboon.png",
        .callback = fetch_callback,
        .buffer_ptr = state.file_buffer,
        .buffer_size = sizeof(state.file_buffer)
    });

    // choose a pseudo-random background color
    float r = (float)((xorshift32() & 0x3F) << 2) / 255.0f;
    float g = (float)((xorshift32() & 0x3F) << 2) / 255.0f;
    float b = (float)((xorshift32() & 0x3F) << 2) / 255.0f;
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { r, g, b, 1.0f } }
    };
}

/* the texture loading callback */
static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        int png_width, png_height, num_channels;
        const int desired_channels = 4;
        stbi_uc* pixels = stbi_load_from_memory(
            response->buffer_ptr,
            (int)response->fetched_size,
            &png_width, &png_height,
            &num_channels, desired_channels);
        if (pixels) {
            sg_init_image(state.bind.fs_images[SLOT_tex], &(sg_image_desc){
                .width = png_width,
                .height = png_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = SG_FILTER_LINEAR,
                .mag_filter = SG_FILTER_LINEAR,
                .content.subimage[0][0] = {
                    .ptr = pixels,
                    .size = png_width * png_height * 4,
                }
            });
            stbi_image_free(pixels);
        }
    }
    else if (response->failed) {
        // if loading the file failed, set clear color to red
        state.pass_action = (sg_pass_action) {
            .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
        };
    }
}

static void frame(void) {
    const int w = sapp_width();
    const int h = sapp_height();
    const float aspect = (float)w / (float)h;

    // pump the sokol-fetch message queues, and invoke response callbacks
    sfetch_dowork();

    // print current memtracker state
    {
        sdtx_canvas(w * 0.5f, h * 0.5f);
        sdtx_origin(1, 1);
        sdtx_puts("PRESS 'SPACE' TO RESTART!\n\n");
        sdtx_puts("Sokol Header Allocations:\n\n");
        sdtx_printf("  Num:  %d\n", smemtrack_info().num_allocs);
        sdtx_printf("  Size: %d\n", smemtrack_info().num_bytes);
    }

    // do some sokol-gl rendering
    {
        /* compute viewport rectangles so that the views are horizontally
           centered and keep a 1:1 aspect ratio
        */
        const int x0 = (w - h)/2;
        const int y0 = 0;

        sgl_viewport(x0, y0, h, h, true);
        sgl_defaults();
        sgl_begin_triangles();
        sgl_matrix_mode_modelview();
        sgl_translate(sinf(state.rx * 0.05f) * 0.5f, cosf(state.rx * 0.1f) * 0.5f, 0.0f);
        sgl_scale(0.5f, 0.5f, 1.0f);
        sgl_v2f_c3b( 0.0f,  0.5f, 255, 0, 0);
        sgl_v2f_c3b(-0.5f, -0.5f, 0, 0, 255);
        sgl_v2f_c3b( 0.5f, -0.5f, 0, 255, 0);
        sgl_end();
        sgl_viewport(0, 0, w, h, true);
    }

    // compute model-view-projection matrix for vertex shader
    hmm_mat4 proj = HMM_Perspective(60.0f, aspect, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sgl_draw();
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sdtx_shutdown();
    sgl_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (ev->key_code == SAPP_KEYCODE_SPACE) {
            cleanup();
            init();
        }
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .gl_force_gles2 = true,
        .window_title = "Restart Sokol Libs (sokol-app)",
    };
}
