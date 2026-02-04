//------------------------------------------------------------------------------
//  blend-playground-sapp.c
//
//  Test/demonstrate blend state configuration.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#include "util/fileutil.h"
#include "qoi/qoi.h"
#include "blend-playground-sapp.glsl.h"

#define MAX_FILE_SIZE (768 * 1024)

static struct {
    sg_pass_action pass_action;
    sg_pipeline bg_pip;
    struct {
        bool valid;
        sg_image img;
        sg_view tex_view;
        sg_sampler smp;
        sg_pipeline pip;
        float width;
        float height;
        float scale;
        struct { float x, y; } offset;
    } image;
    struct {
        sfetch_error_t error;
        bool qoi_decode_failed;
        uint8_t buf[MAX_FILE_SIZE];
    } file;
} state;

static void draw_ui(void);
static void fetch_callback(const sfetch_response_t* response);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgimgui_setup(&(sgimgui_desc_t){0});
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 1,
        .num_channels = 1,
        .num_lanes = 1,
        .logger.func = slog_func,
    });

    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };

    // create pipeline and shader to draw a bufferless fullscreen triangle as background
    state.bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(bg_shader_desc(sg_query_backend())),
        .label = "background-pipeline"
    });

    // start loading example texture
    char path_buf[512];
    state.file.error = SFETCH_ERROR_NO_ERROR;
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("dice.qoi", path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(state.file.buf),
    });
}

static void frame(void) {
    sfetch_dowork();
    draw_ui();

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });

    // draw background
    const bg_params_t bg_params = { .light = 0.6f, .dark = 0.4f };
    sg_apply_pipeline(state.bg_pip);
    sg_apply_uniforms(UB_bg_params, &SG_RANGE(bg_params));
    sg_draw(0, 3, 1);

    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void draw_ui(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        igEndMainMenuBar();
    }
    sgimgui_draw();
}

static void cleanup(void) {
    sfetch_shutdown();
    simgui_shutdown();
    sgimgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    if (simgui_handle_event(ev)) {
        return;
    }
    // FIXME: handle panning
}

static void create_image(const void* qoi_data_ptr, size_t qoi_data_size) {
    if (state.image.img.id != SG_INVALID_ID) {
        sg_destroy_image(state.image.img);
        state.image.img.id = SG_INVALID_ID;
    }
    if (state.image.tex_view.id != SG_INVALID_ID) {
        sg_destroy_view(state.image.tex_view);
        state.image.tex_view.id = SG_INVALID_ID;
    }
    state.image.valid = false;
    qoi_desc qoi;
    void* pixels = qoi_decode(qoi_data_ptr, (int)qoi_data_size, &qoi, 4);
    if (!pixels) {
        state.file.qoi_decode_failed = true;
        return;
    }
    state.image.width = (float)qoi.width;
    state.image.height = (float)qoi.height;
    state.image.img = sg_make_image(&(sg_image_desc){
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .width = (int)qoi.width,
        .height = (int)qoi.height,
        .data.mip_levels[0] = {
            .ptr = pixels,
            .size = qoi.width * qoi.height * 4,
        },
        .label = "qoi-image",
    });
    free(pixels);
    state.image.tex_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = state.image.img },
        .label = "qoi-image-texview",
    });
    state.image.valid = true;
}

static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        state.file.error = SFETCH_ERROR_NO_ERROR;
        create_image(response->data.ptr, response->data.size);
    } else if (response->failed) {
        state.file.error = response->error_code;
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
        .sample_count = 1,
        .window_title = "blend-playground-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
