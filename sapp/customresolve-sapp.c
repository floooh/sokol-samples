//------------------------------------------------------------------------------
//  customresolve-sapp.c
//
//  Demonstrate custom MSAA resolve in a render pass which reads individual
//  MSAA samples in the fragment shader.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_IMGUI_IMPL
#define SOKOL_GFX_IMGUI_IMPL
#include "cimgui.h"
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"
#include "customresolve-sapp.glsl.h"

#define WIDTH (160)
#define HEIGHT (120)

static struct {
    struct {
        sg_image img;
        sg_view tex_view;
        sg_pipeline pip;
        sg_pass pass;
    } msaa;
    struct {
        sg_image img;
        sg_view tex_view;
        sg_pipeline pip;
        sg_pass pass;
        sg_bindings bind;
        fs_params_t fs_params;
    } resolve;
    struct {
        sg_pipeline pip;
        sg_pass_action action;
        sg_bindings bind;
    } display;
    sg_sampler smp; // a common non-filtering sampler
} state;

const fs_params_t default_weights = {
    .weight0 = 0.25f,
    .weight1 = 0.25f,
    .weight2 = 0.25f,
    .weight3 = 0.25f,
};

static void draw_fallback(void);
static void draw_ui(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgimgui_setup(&(sgimgui_desc_t){0});
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });

    // catch WebGL2/GLES3
    if (!sg_query_features().msaa_texture_bindings) {
        return;
    }

    // common objects
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // msaa-render-pass objects
    state.msaa.img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = WIDTH,
        .height = HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 4,
        .label = "msaa-image",
    });
    state.msaa.tex_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = state.msaa.img },
        .label = "msaa-texture-view",
    });
    state.msaa.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(msaa_shader_desc(sg_query_backend())),
        .sample_count = 4,
        .depth.pixel_format = SG_PIXELFORMAT_NONE,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "msaa-pipeline",
    });
    state.msaa.pass = (sg_pass) {
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .store_action = SG_STOREACTION_STORE,
                .clear_value = { 0, 0, 0, 1 },
            },
        },
        .attachments.colors[0] = sg_make_view(&(sg_view_desc){
            .color_attachment = { .image = state.msaa.img },
            .label = "msaa-attachment-view",
        }),
    };

    // resolve-render-pass objects
    state.resolve.img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = WIDTH,
        .height = HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "resolve-image",
    });
    state.resolve.tex_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = state.resolve.img },
        .label = "resolve-texture-view",
    });
    state.resolve.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(resolve_shader_desc(sg_query_backend())),
        .sample_count = 1,
        .depth.pixel_format = SG_PIXELFORMAT_NONE,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "resolve-pipeline",
    });
    state.resolve.pass = (sg_pass){
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_DONTCARE,
                .store_action = SG_STOREACTION_STORE
            },
        },
        .attachments.colors[0] = sg_make_view(&(sg_view_desc){
            .color_attachment = { .image = state.resolve.img },
            .label = "resolve-attachments-view",
        }),
    };
    state.resolve.bind = (sg_bindings){
        .views[VIEW_texms] = state.msaa.tex_view,
        .samplers[SMP_smp] = state.smp,
    };
    state.resolve.fs_params = default_weights;

    // swapchain-render-pass objects
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label = "display pipeline",
    });
    state.display.action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_DONTCARE }
    };
    state.display.bind = (sg_bindings){
        .views[VIEW_tex] = state.resolve.tex_view,
        .samplers[SMP_smp] = state.smp,
    };
}

static void frame(void) {
    draw_ui();
    if (!sg_query_features().msaa_texture_bindings) {
        draw_fallback();
        return;
    }

    // draw a triangle into an msaa render target
    sg_begin_pass(&state.msaa.pass);
    sg_apply_pipeline(state.msaa.pip);
    sg_draw(0, 3, 1);
    sg_end_pass();

    // custom resolve pass (via a 'fullscreen triangle')
    sg_begin_pass(&state.resolve.pass);
    sg_apply_pipeline(state.resolve.pip);
    sg_apply_bindings(&state.resolve.bind);
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(state.resolve.fs_params));
    sg_draw(0, 3, 1);
    sg_end_pass();

    // the final swapchain pass (also via a 'fullscreen triangle')
    sg_begin_pass(&(sg_pass){ .action = state.display.action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_draw(0, 3, 1);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sgimgui_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

static void draw_ui(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .dpi_scale = sapp_dpi_scale(),
        .delta_time = sapp_frame_duration(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        igEndMainMenuBar();
    }
    sgimgui_draw();

    igSetNextWindowPos((ImVec2){10, 20}, ImGuiCond_Once);
    if (igBegin("#window", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoBackground)) {
        if (sg_query_features().msaa_texture_bindings) {
            igText("Sample Weights:");
            igSliderFloatEx("0", &state.resolve.fs_params.weight0, 0.0f, 1.0f, "%.2f", 0);
            igSliderFloatEx("1", &state.resolve.fs_params.weight1, 0.0f, 1.0f, "%.2f", 0);
            igSliderFloatEx("2", &state.resolve.fs_params.weight2, 0.0f, 1.0f, "%.2f", 0);
            igSliderFloatEx("3", &state.resolve.fs_params.weight3, 0.0f, 1.0f, "%.2f", 0);
            igCheckboxFlagsIntPtr("show complex pixels", &state.resolve.fs_params.coverage, 1);
            if (igButton("Reset")) {
                state.resolve.fs_params = default_weights;
            }
        } else {
            igText("MSAA TEXTURES NOT SUPPORTED ON WEBGL2/GLES3/macOS+GL");
        }
    }
    igEnd();
}

static void draw_fallback(void) {
    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0, 0, 1} },
        },
        .swapchain = sglue_swapchain(),
    });
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 640,
        .height = 480,
        .sample_count = 1,
        .window_title = "customresolve-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
