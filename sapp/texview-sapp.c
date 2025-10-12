//------------------------------------------------------------------------------
//  texview-sapp.c
//
//  Test/demonstrate texture-view features.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#include "basisu/sokol_basisu.h"
#include "util/fileutil.h"
#include "texview-sapp.glsl.h"

#define MAX_FILE_SIZE (128 * 1024)
#define NUM_FILES (5)
static const char* files[NUM_FILES] = {
    "kodim05.basis",
    "kodim07.basis",
    "kodim17.basis",
    "kodim20.basis",
    "kodim23.basis",
};

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sg_view tex_view;
    sg_pipeline pip;
    struct {
        sg_sampler linear;
        sg_sampler nearest;
    } smp;
    struct {
        int width;
        int height;
        int num_mipmaps;
    } img_info;
    struct {
        bool pending;
        bool failed;
    } load;
    struct {
        int selected;
        int min_mip;
        int max_mip;
        float mip_lod;
        bool use_linear_sampler;
        sgimgui_t sgimgui;
    } ui;
} state;

static uint8_t file_buffer[MAX_FILE_SIZE];

static void ui_draw(void);
static void fetch_async(const char* filename);
static void fetch_callback(const sfetch_response_t*);
static void reinit_texview(void);
static void apply_viewport(void);
static bool has_texture_views(void);

static void init(void) {
    sbasisu_setup();
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgimgui_init(&state.ui.sgimgui, &(sgimgui_desc_t){0});
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
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    // pre-allocate handles so we can keep rendering even no
    // image has been loaded yet
    state.img = sg_alloc_image();
    state.tex_view = sg_alloc_view();

    // a render pipeline for bufferless 2D-rendering
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(texview_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "pipeline",
    });

    state.smp.linear = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_LINEAR,
        .label = "linear-sampler",
    });
    state.smp.nearest = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .mipmap_filter = SG_FILTER_LINEAR,
        .label = "nearest-sampler",
    });

    // start loading the first image
    fetch_async(files[0]);
}

static void frame(void) {
    sfetch_dowork();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    ui_draw();

    const fs_params_t fs_params = { .mip_lod = state.ui.mip_lod };
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    apply_viewport();
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_tex] = state.tex_view,
        .samplers[SMP_smp] = state.ui.use_linear_sampler ? state.smp.linear : state.smp.nearest,
    });
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(fs_params));
    sg_draw(0, 4, 1);
    sgimgui_draw(&state.ui.sgimgui);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

static void cleanup(void) {
    sfetch_shutdown();
    sgimgui_discard(&state.ui.sgimgui);
    simgui_shutdown();
    sg_shutdown();
    sbasisu_shutdown();
}

static void ui_draw(void) {
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu(&state.ui.sgimgui, "sokol-gfx");
        igEndMainMenuBar();
    }
    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        if (state.load.pending) {
            igText("Loading ...");
        } else {
            if (state.load.failed) {
                igText("Loading failed!");
            }
            if (!has_texture_views()) {
                igText("NOTE: WebGL2/GLES3/GL4.1 have no texture views!\n");
            }
            if (igComboChar("Image", &state.ui.selected, files, IM_ARRAYSIZE(files))) {
                fetch_async(files[state.ui.selected]);
            }
            igText("Width:   %d", state.img_info.width);
            igText("Height:  %d", state.img_info.height);
            igText("Mipmaps: %d", state.img_info.num_mipmaps);
            igSeparator();
            igCheckbox("Use Linear Sampler", &state.ui.use_linear_sampler);
            float max_mip_lod = (float)(state.ui.max_mip - state.ui.min_mip);
            if (state.ui.mip_lod > max_mip_lod) {
                state.ui.mip_lod = max_mip_lod;
            }
            igSliderFloat("Mip LOD", &state.ui.mip_lod, 0.0f, (float)(state.ui.max_mip - state.ui.min_mip));
            if (igSliderInt("Min Mip", &state.ui.min_mip, 0, (state.img_info.num_mipmaps - 1))) {
                if (state.ui.max_mip < state.ui.min_mip) {
                    state.ui.max_mip = state.ui.min_mip;
                }
                reinit_texview();
            }
            if (igSliderInt("Max Mip", &state.ui.max_mip, 0, (state.img_info.num_mipmaps - 1))) {
                if (state.ui.min_mip > state.ui.max_mip) {
                    state.ui.min_mip = state.ui.max_mip;
                }
                reinit_texview();
            }
        }
    }
    igEnd();
}

static void fetch_async(const char* filename) {
    state.load.pending = false;
    state.load.failed = false;
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path(filename, path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(file_buffer),
    });
}

static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load.pending = false;
        sg_uninit_image(state.img);
        const sg_image_desc img_desc = sbasisu_transcode((sg_range){
            .ptr = response->data.ptr,
            .size = response->data.size,
        });
        assert(img_desc.num_mipmaps > 0);
        state.img_info.width = img_desc.width;
        state.img_info.height = img_desc.height;
        state.img_info.num_mipmaps = img_desc.num_mipmaps;
        state.ui.min_mip = 0;
        state.ui.max_mip = img_desc.num_mipmaps - 1;
        state.ui.mip_lod = 0.0f;
        sg_init_image(state.img, &img_desc);
        sbasisu_free(&img_desc);
        reinit_texview();
    } else if (response->failed) {
        state.load.failed = true;
    }
}

static void reinit_texview(void) {
    sg_uninit_view(state.tex_view);
    sg_init_view(state.tex_view, &(sg_view_desc){
        .texture = {
            .image = state.img,
            .mip_levels = {
                .base = state.ui.min_mip,
                .count = (state.ui.max_mip - state.ui.min_mip) + 1,
            },
        },
    });
}

static bool has_texture_views(void) {
    const sg_backend backend = sg_query_backend();
    return !((backend == SG_BACKEND_GLCORE || backend == SG_BACKEND_GLES3) && !sg_query_features().gl_texture_views);
}

// set viewport to keep image aspect ratio correct regardless of window size
static void apply_viewport(void) {
    if ((state.img_info.width == 0) || (state.img_info.height == 0)) {
        return;
    }
    const float border = 5.0f;
    float canvas_width = sapp_widthf() - 2.0f * border;
    float canvas_height = sapp_heightf() - 2.0f * border;
    if (canvas_width < 1.0f) {
        canvas_width = 1.0f;
    }
    if (canvas_height < 1.0f) {
        canvas_height = 1.0f;
    }
    const float canvas_aspect = canvas_width / canvas_height;
    const float img_width = (float)state.img_info.width;
    const float img_height = (float)state.img_info.height;
    const float img_aspect = img_width / img_height;
    float vp_x, vp_y, vp_w, vp_h;
    if (img_aspect < canvas_aspect) {
        vp_y = border;
        vp_h = canvas_height;
        vp_w = canvas_height * img_aspect;
        vp_x = border + (canvas_width - vp_w) * 0.5f;
    } else {
        vp_x = border;
        vp_w = canvas_width;
        vp_h = canvas_width / img_aspect;
        vp_y = border + (canvas_height - vp_h) * 0.5f;
    }
    sg_apply_viewportf(vp_x, vp_y, vp_w, vp_h, true);
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
        .window_title = "texview-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
