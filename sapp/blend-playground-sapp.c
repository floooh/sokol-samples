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
#define NUM_BLEND_FACTORS (19)
#define NUM_BLEND_OPS (5)

static const char* blend_factor_names[NUM_BLEND_FACTORS] = {
    "Zero",
    "One",
    "SrcColor",
    "OneMinusSrcColor",
    "SrcAlpha",
    "OneMinusSrcAlpha",
    "DstColor",
    "OneMinusDstColor",
    "DstAlpha",
    "OneMinusDstAlpha",
    "SrcAlphaSaturated",
    "BlendColor",
    "OneMinusBlendColor",
    "BlendAlpha",
    "OneMinusBlendAlpha",
    "Src1Color",
    "OneMinusSrc1Color",
    "Src1Alpha",
    "OneMinusSrc1Alpha",
};

static const sg_blend_factor blend_factors[NUM_BLEND_FACTORS] = {
    SG_BLENDFACTOR_ZERO,
    SG_BLENDFACTOR_ONE,
    SG_BLENDFACTOR_SRC_COLOR,
    SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
    SG_BLENDFACTOR_SRC_ALPHA,
    SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    SG_BLENDFACTOR_DST_COLOR,
    SG_BLENDFACTOR_ONE_MINUS_DST_COLOR,
    SG_BLENDFACTOR_DST_ALPHA,
    SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
    SG_BLENDFACTOR_SRC_ALPHA_SATURATED,
    SG_BLENDFACTOR_BLEND_COLOR,
    SG_BLENDFACTOR_ONE_MINUS_BLEND_COLOR,
    SG_BLENDFACTOR_BLEND_ALPHA,
    SG_BLENDFACTOR_ONE_MINUS_BLEND_ALPHA,
    SG_BLENDFACTOR_SRC1_COLOR,
    SG_BLENDFACTOR_ONE_MINUS_SRC1_COLOR,
    SG_BLENDFACTOR_SRC1_ALPHA,
    SG_BLENDFACTOR_ONE_MINUS_SRC1_ALPHA,
};

static const char* blend_op_names[NUM_BLEND_OPS] = {
    "Add",
    "Subtract",
    "ReverseSubtract",
    "Min",
    "Max",
};

static const sg_blend_op blend_ops[NUM_BLEND_OPS] = {
    SG_BLENDOP_ADD,
    SG_BLENDOP_SUBTRACT,
    SG_BLENDOP_REVERSE_SUBTRACT,
    SG_BLENDOP_MIN,
    SG_BLENDOP_MAX,
};

enum {
    SHADER_STD = 0,
    NUM_SHADERS,
};

static struct {
    sg_pass_action pass_action;
    sg_pipeline bg_pip;
    struct {
        bool valid;
        sg_image img;
        sg_view tex_view;
        sg_sampler smp;
        sg_pipeline pip;
        sg_shader shaders[NUM_SHADERS];
        float width;
        float height;
    } image;
    struct {
        sg_blend_state blend;
        float alpha_scale;
        float scale;
        vec2_t offset;
    } ctrl;
    struct {
        int src_factor_rgb_sel;
        int dst_factor_rgb_sel;
        int op_rgb_sel;
        int src_factor_alpha_sel;
        int dst_factor_alpha_sel;
        int op_alpha_sel;
    } ui;
    struct {
        sfetch_error_t error;
        bool qoi_decode_failed;
        uint8_t buf[MAX_FILE_SIZE];
    } file;
} state;

static bool draw_ui(void);
static void fetch_callback(const sfetch_response_t* response);
static void recreate_pipeline(void);
static img_params_t compute_image_params(void);

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

    // initial control state
    state.ctrl.blend.enabled = true;
    state.ctrl.blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
    state.ctrl.blend.dst_factor_rgb = SG_BLENDFACTOR_ZERO;
    state.ctrl.blend.op_rgb = SG_BLENDOP_ADD;
    state.ctrl.blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    state.ctrl.blend.dst_factor_alpha = SG_BLENDFACTOR_ZERO;
    state.ctrl.blend.op_alpha = SG_BLENDOP_ADD;
    state.ctrl.alpha_scale = 1.0f;
    state.ctrl.scale = 0.75;

    // initial UI state
    state.ui.src_factor_rgb_sel = 0;
    state.ui.dst_factor_rgb_sel = 1;
    state.ui.src_factor_alpha_sel = 0;
    state.ui.dst_factor_alpha_sel = 1;
    state.ui.op_rgb_sel = 0;
    state.ui.op_alpha_sel = 0;

    // create pipeline and shader to draw a bufferless fullscreen triangle as background
    state.bg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(bg_shader_desc(sg_query_backend())),
        .label = "background-pipeline"
    });

    // create image resources
    state.image.shaders[SHADER_STD] = sg_make_shader(img_std_shader_desc(sg_query_backend()));
    state.image.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "sampler",
    });
    recreate_pipeline();

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
    bool pip_dirty = draw_ui();
    if (pip_dirty) {
        recreate_pipeline();
    }

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });

    // draw background
    const bg_params_t bg_params = { .light = 0.6f, .dark = 0.4f };
    sg_apply_pipeline(state.bg_pip);
    sg_apply_uniforms(UB_bg_params, &SG_RANGE(bg_params));
    sg_draw(0, 3, 1);

    // draw image
    if (state.image.valid) {
        const img_params_t img_params = compute_image_params();
        sg_apply_pipeline(state.image.pip);
        sg_apply_bindings(&(sg_bindings){
            .views[VIEW_tex] = state.image.tex_view,
            .samplers[SMP_smp] = state.image.smp,
        });
        sg_apply_uniforms(UB_img_params, &SG_RANGE(img_params));
        sg_draw(0, 4, 1);
    }

    simgui_render();
    sg_end_pass();
    sg_commit();
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
    // FIXME: handle pan/scale
}

static bool draw_ui(void) {
    bool pip_dirty = false;
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
    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        if (state.file.error != SFETCH_ERROR_NO_ERROR) {
            igText("Failed to load image.");
        } else if (!state.image.valid) {
            igText("Loading image...");
        } else {
            igSliderFloat("Image Scale", &state.ctrl.scale, 0.1f, 10.0f);
            igSliderFloat("Alpha Scale", &state.ctrl.alpha_scale, 0.0f, 1.0f);
            if (igComboChar("Src Factor RGB", &state.ui.src_factor_rgb_sel, blend_factor_names, NUM_BLEND_FACTORS)) {
                state.ctrl.blend.src_factor_rgb = blend_factors[state.ui.src_factor_rgb_sel];
                pip_dirty = true;
            }
            if (igComboChar("Dst Factor RGB", &state.ui.dst_factor_rgb_sel, blend_factor_names, NUM_BLEND_FACTORS)) {
                state.ctrl.blend.dst_factor_rgb = blend_factors[state.ui.dst_factor_rgb_sel];
                pip_dirty = true;
            }
            if (igComboChar("Op RGB", &state.ui.op_rgb_sel, blend_op_names, NUM_BLEND_OPS)) {
                state.ctrl.blend.op_rgb = blend_ops[state.ui.op_rgb_sel];
                pip_dirty = true;
            }
            if (igComboChar("Src Factor Alpha", &state.ui.src_factor_alpha_sel, blend_factor_names, NUM_BLEND_FACTORS)) {
                state.ctrl.blend.src_factor_alpha = blend_factors[state.ui.src_factor_alpha_sel];
                pip_dirty = true;
            }
            if (igComboChar("Dst Factor Alpha", &state.ui.dst_factor_alpha_sel, blend_factor_names, NUM_BLEND_FACTORS)) {
                state.ctrl.blend.dst_factor_alpha = blend_factors[state.ui.dst_factor_alpha_sel];
                pip_dirty = true;
            }
            if (igComboChar("Op Alpha", &state.ui.op_alpha_sel, blend_op_names, NUM_BLEND_OPS)) {
                state.ctrl.blend.op_alpha = blend_ops[state.ui.op_alpha_sel];
                pip_dirty = true;
            }
        }
    }
    igEnd();
    return pip_dirty;
}

static void recreate_pipeline(void) {
    if (state.image.pip.id != SG_INVALID_ID) {
        sg_destroy_pipeline(state.image.pip);
        state.image.pip.id = SG_INVALID_ID;
    }

    // synthesize vertices in vertex shader
    state.image.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = state.image.shaders[SHADER_STD],
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .color_count = 1,
        .colors[0].blend = state.ctrl.blend,
        .label = "img-pipeline",
    });

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

static img_params_t compute_image_params(void) {
    return (img_params_t){
        .offset = state.ctrl.offset,
        .scale = {
            .x = (state.image.width / sapp_widthf()) * state.ctrl.scale,
            .y = (state.image.height / sapp_heightf()) * state.ctrl.scale,
        },
        .alpha_scale = state.ctrl.alpha_scale,
    };
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
