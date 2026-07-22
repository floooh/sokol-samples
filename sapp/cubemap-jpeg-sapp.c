//------------------------------------------------------------------------------
//  cubemap-jpeg-sapp.c
//
//  Load and render cubemap from individual jpeg files. Also demonstrates
//  how to incrementally populate an image via sg_write_image_unsealed()
//------------------------------------------------------------------------------
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_fetch.h"
#include "sokol_debugtext.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "util/camera.h"
#include "util/fileutil.h"
#include "cubemap-jpeg-sapp.glsl.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sg_pipeline pip;
    sg_bindings bind;
    camera_t camera;
    int load_count;
    bool load_failed;
} state;

#define NUM_FACES (6)
#define FACE_WIDTH (2048)
#define FACE_HEIGHT (2048)
#define FACE_NUM_BYTES (FACE_WIDTH * FACE_HEIGHT * 4)

static uint8_t iobuffer[FACE_NUM_BYTES];

static void fetch_cb(const sfetch_response_t*);

static void init(void) {

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric(),
        .logger.func = slog_func,
    });

    // setup sokol-fetch, restrict to one download at a time
    // to preserve memory and demonstrate writing individual
    // cubemap faces vis sg_write_image_unsealed()
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 6,
        .num_channels = 1,
        .num_lanes = 1,
        .logger.func = slog_func,
    });

    // setup camera helper
    cam_init(&state.camera, &(camera_desc_t){
        .latitude = 0.0f,
        .longitude = 0.0f,
        .distance = 0.1f,
        .min_dist = 0.1f,
        .max_dist = 0.1f,
    });

    // pass action, clear to black
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1 } },
    };

    const float vertices[] = {
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cubemap-vertices"
    });

    const uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "cubemap-indices"
    });

    // create cubemap image without initial content in unsealed state
    state.img = sg_make_image(&(sg_image_desc){
        .usage.write_unsealed = true,
        .type = SG_IMAGETYPE_CUBE,
        .width = FACE_WIDTH,
        .height = FACE_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "cubemap-image",
    });

    // create texture view for the cubemap image
    state.bind.views[VIEW_tex] = sg_make_view(&(sg_view_desc){
        .texture = { .image = state.img },
        .label = "cubemap-view",
    });

    // a sampler object
    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .label = "cubemap-sampler"
    });

    // a pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout.attrs[ATTR_cubemap_pos].format = SG_VERTEXFORMAT_FLOAT3,
        .shader = sg_make_shader(cubemap_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .label = "cubemap-pipeline",
    });

    // start loading the six cubemap face image files, note that the loading
    // will be throttled to happen in sequence by sokol-fetch, that way it's
    // safe to reuse the same iobuffer for each image
    char path_buf[1024];
    const char* filenames[NUM_FACES] = {
        "nb2_posx.jpg", "nb2_negx.jpg",
        "nb2_posy.jpg", "nb2_negy.jpg",
        "nb2_posz.jpg", "nb2_negz.jpg"
    };
    for (int face_index = 0; face_index < NUM_FACES; face_index++) {
        sfetch_send(&(sfetch_request_t){
            .path = fileutil_get_path(filenames[face_index], path_buf, sizeof(path_buf)),
            .callback = fetch_cb,
            .buffer = SFETCH_RANGE(iobuffer),
            .user_data = SFETCH_RANGE(face_index),
        });
    }
}

static void fetch_cb(const sfetch_response_t* response) {
    if (response->fetched) {
        // decode loaded jpeg data
        int width, height, channels_in_file;
        const int desired_channels = 4;
        stbi_uc* decoded_pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &width, &height,
            &channels_in_file, desired_channels);
        if (decoded_pixels) {
            assert(width == FACE_WIDTH);
            assert(height == FACE_HEIGHT);

            // get the cubeface index from response's userdata
            assert(response->user_data);
            int face_index = *(int*)response->user_data;

            // write the cubeface data into the image
            sg_write_image_unsealed(&(sg_write_image_desc){
                // note: src.bytes_per_row and src.rows_per_slice can remain
                // at default zero because the in-memory data is already in the right layout
                .src.data = {
                    .ptr = decoded_pixels,
                    .size = FACE_NUM_BYTES
                },
                .dst = {
                    .image = state.img,
                    .mip_level = 0,
                    .slice = face_index
                },
                // FIXME: test with width = 0, height = 0
                .size = {
                    .width = width,
                    .height = height,
                    .num_slices = 1,
                },
            });
            stbi_image_free(decoded_pixels);

            // when all 6 cube faces have been loaded, seal the image
            if (++state.load_count == NUM_FACES) {
                sg_seal_image(state.img);
            }
        }
    } else if (response->failed) {
        state.load_failed = true;
    }
}

static void frame(void) {
    sfetch_dowork();
    cam_update(&state.camera, sapp_width(), sapp_height());

    const vs_params_t vs_params = {
        .mvp = state.camera.view_proj,
    };

    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_origin(1, 1);
    if (state.load_failed) {
        sdtx_puts("LOAD FAILED!");
    } else if (state.load_count < 6) {
        sdtx_puts("LOADING ...");
    } else {
        sdtx_puts("LMB + move mouse to look around");
    }

    // NOTE: as long as the image is in unsealed state (e.g. data is still loading)
    // all render operations involving the image will silently be skipped
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind),
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sfetch_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    if (__dbgui_event_with_retval(ev)) {
        return;
    }
    cam_handle_event(&state.camera, ev);
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
        .sample_count = 1,
        .window_title = "cubemap-jpeg-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
