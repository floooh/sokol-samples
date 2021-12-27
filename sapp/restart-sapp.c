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
#include <string.h> // memset

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_audio.h"
#include "sokol_fetch.h"
#include "sokol_gl.h"
#include "sokol_debugtext.h"
#include "sokol_memtrack.h"
#include "sokol_glue.h"
#include "stb/stb_image.h"

#include "modplug.h"

#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "restart-sapp.glsl.h"

#define MOD_NUM_CHANNELS (2)
#define MOD_SRCBUF_SAMPLES (16*1024)

static struct {
    bool reset;
    struct {
        float rx, ry;
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } scene;
    struct {
        ModPlugFile* mpf;
        int int_buf[MOD_SRCBUF_SAMPLES];
        float flt_buf[MOD_SRCBUF_SAMPLES];
    } mod;
    struct {
        uint8_t img_buffer[256 * 1024];
        uint8_t mod_buffer[512 * 1024];
    } io;
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

static void fetch_img_callback(const sfetch_response_t*);
static void fetch_mod_callback(const sfetch_response_t*);

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

// the sokol-app init callback is also called when restarting
static void init(void) {

    // setup sokol libraries (tweak setup params to reduce memory usage)
    sg_setup(&(sg_desc){
        .buffer_pool_size = 8,
        .image_pool_size = 4,
        .shader_pool_size = 4,
        .pipeline_pool_size = 8,
        .pass_pool_size = 1,
        .context_pool_size = 1,
        .sampler_cache_size = 4,
        .context = sapp_sgcontext()
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 2,
        .num_channels = 2,
        .num_lanes = 1
    });
    sgl_setup(&(sgl_desc_t){
        .pipeline_pool_size = 1,
        .max_vertices = 16,
        .max_commands = 16,
    });
    sdtx_setup(&(sdtx_desc_t){
        .context_pool_size = 1,
        .printf_buf_size = 128,
        .fonts[0] = sdtx_font_cpc(),
        .context = {
            .char_buf_size = 128,
        }
    });
    saudio_setup(&(saudio_desc){
        .num_channels = MOD_NUM_CHANNELS,
    });

    // setup rendering resources
    state.scene.bind.fs_images[0] = sg_alloc_image();

    state.scene.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices),
        .label = "cube-vertices"
    });

    state.scene.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(cube_indices),
        .label = "cube-indices"
    });

    state.scene.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(restart_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_texcoord0].format = SG_VERTEXFORMAT_SHORT2N
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .label = "cube-pipeline"
    });

    // set a pseudo-random background color on each restart
    float r = (float)((xorshift32() & 0x3F) << 2) / 255.0f;
    float g = (float)((xorshift32() & 0x3F) << 2) / 255.0f;
    float b = (float)((xorshift32() & 0x3F) << 2) / 255.0f;
    state.scene.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR,.value = { r, g, b, 1.0f } }
    };

    // initialize libmodplug
    ModPlug_Settings mps;
    ModPlug_GetSettings(&mps);
    mps.mChannels = saudio_channels();
    mps.mBits = 32;
    mps.mFrequency = saudio_sample_rate();
    mps.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
    mps.mMaxMixChannels = 64;
    mps.mLoopCount = -1;
    mps.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
    ModPlug_SetSettings(&mps);

    // start loading files
    sfetch_send(&(sfetch_request_t){
        .path = "baboon.png",
        .callback = fetch_img_callback,
        .buffer_ptr = state.io.img_buffer,
        .buffer_size = sizeof(state.io.img_buffer)
    });
    sfetch_send(&(sfetch_request_t){
        .path = "comsi.s3m",
        .callback = fetch_mod_callback,
        .buffer_ptr = state.io.mod_buffer,
        .buffer_size = sizeof(state.io.mod_buffer)
    });
}

// file sokol-fetch loading callbacks
static void fetch_img_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        int png_width, png_height, num_channels;
        const int desired_channels = 4;
        stbi_uc* pixels = stbi_load_from_memory(
            response->buffer_ptr,
            (int)response->fetched_size,
            &png_width, &png_height,
            &num_channels, desired_channels);
        if (pixels) {
            sg_init_image(state.scene.bind.fs_images[SLOT_tex], &(sg_image_desc){
                .width = png_width,
                .height = png_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = SG_FILTER_LINEAR,
                .mag_filter = SG_FILTER_LINEAR,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(png_width * png_height * 4),
                }
            });
            stbi_image_free(pixels);
        }
    }
    else if (response->failed) {
        // if loading the file failed, set clear color to red
        state.scene.pass_action = (sg_pass_action) {
            .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.0f, 0.0f, 1.0f } }
        };
    }
}

static void fetch_mod_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        state.mod.mpf = ModPlug_Load(response->buffer_ptr, (int)response->fetched_size);
    }
    else if (response->failed) {
        // if loading the file failed, set clear color to red
        state.scene.pass_action = (sg_pass_action) {
            .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.0f, 0.0f, 1.0f } }
        };
    }
}

// the sokol-app cleanup callback is also called on restart
static void shutdown(void) {
    if (state.mod.mpf) {
        ModPlug_Unload(state.mod.mpf);
    }
    saudio_shutdown();
    sdtx_shutdown();
    sgl_shutdown();
    sfetch_shutdown();
    sg_shutdown();
    memset(&state, 0, sizeof(state));
}

// the sokol-app frame callback
static void frame(void) {
    if (state.reset) {
        state.reset = false;
        shutdown();
        init();
    }

    const int w = sapp_width();
    const int h = sapp_height();
    const float aspect = (float)w / (float)h;
    const float t = (float)(sapp_frame_duration() * 60.0);

    // pump the sokol-fetch message queues, and invoke response callbacks
    sfetch_dowork();

    // print current memtracker state
    {
        sdtx_canvas(w * 0.5f, h * 0.5f);
        sdtx_origin(1, 1);
        sdtx_puts("PRESS 'SPACE' TO RESTART!\n\n");
        sdtx_puts("Sokol Header Allocations:\n\n");
        sdtx_printf("  Num:  %d\n", smemtrack_info().num_allocs);
        sdtx_printf("  Size: %d bytes\n\n", smemtrack_info().num_bytes);
        sdtx_printf("MOD: Combat Signal by ???");
    }

    // play audio
    const int num_frames = saudio_expect();
    if (num_frames > 0) {
        int num_samples = num_frames * saudio_channels();
        if (num_samples > MOD_SRCBUF_SAMPLES) {
            num_samples = MOD_SRCBUF_SAMPLES;
        }
        if (state.mod.mpf) {
            /* NOTE: for multi-channel playback, the samples are interleaved
               (e.g. left/right/left/right/...)
            */
            int res = ModPlug_Read(state.mod.mpf, (void*)state.mod.int_buf, (int)sizeof(int)*num_samples);
            int samples_in_buffer = res / (int)sizeof(int);
            int i;
            for (i = 0; i < samples_in_buffer; i++) {
                state.mod.flt_buf[i] = state.mod.int_buf[i] / (float)0x7fffffff;
            }
            for (; i < num_samples; i++) {
                state.mod.flt_buf[i] = 0.0f;
            }
        }
        else {
            /* if file wasn't loaded, fill the output buffer with silence */
            for (int i = 0; i < num_samples; i++) {
                state.mod.flt_buf[i] = 0.0f;
            }
        }
        saudio_push(state.mod.flt_buf, num_frames);
    }

    // do some sokol-gl rendering
    {
        const int x0 = (w - h)/2;
        const int y0 = 0;

        sgl_defaults();
        sgl_viewport(x0, y0, h, h, true);
        sgl_matrix_mode_modelview();
        sgl_translate(sinf(state.scene.rx * 0.05f) * 0.5f, cosf(state.scene.rx * 0.1f) * 0.5f, 0.0f);
        sgl_scale(0.5f, 0.5f, 1.0f);
        sgl_begin_triangles();
        sgl_v2f_c3b( 0.0f,  0.5f, 255, 0, 0);
        sgl_v2f_c3b(-0.5f, -0.5f, 0, 0, 255);
        sgl_v2f_c3b( 0.5f, -0.5f, 0, 255, 0);
        sgl_end();
        sgl_viewport(0, 0, w, h, true);
    }

    // compute model-view-projection matrix for the 3D scene
    hmm_mat4 proj = HMM_Perspective(60.0f, aspect, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
    state.scene.rx += 1.0f * t; state.scene.ry += 2.0f * t;
    hmm_mat4 rxm = HMM_Rotate(state.scene.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.scene.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    // and finally the actual sokol-gfx render pass
    sg_begin_default_pass(&state.scene.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.scene.pip);
    sg_apply_bindings(&state.scene.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sgl_draw();
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

// the sokol-app event callback, performs a "restart" when the SPACE key is pressed
static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (ev->key_code == SAPP_KEYCODE_SPACE) {
            state.reset = true;
        }
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = shutdown,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .gl_force_gles2 = true,
        .window_title = "Restart Sokol Libs (sokol-app)",
        .icon.sokol_default = true,
    };
}
