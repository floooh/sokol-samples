//------------------------------------------------------------------------------
//  srgb-offscreen-sapp.c
//
//  Render into two SRGB offscreen render targets (one with MSAA, the other
//  without MSAA), and compose the result into a SRGB framebuffer.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "srgb-offscreen-sapp.glsl.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"

#define OFFSCREEN_WIDTH (256)
#define OFFSCREEN_HEIGHT (128)
#define MSAA_SAMPLE_COUNT (4)

static struct {
    struct {
        sg_image img;
        sg_view tex_view;
        sg_pass pass;
        sg_pipeline pip;
    } nomsaa;
    struct {
        sg_image msaa_img;
        sg_image resolve_img;
        sg_view tex_view;
        sg_pass pass;
        sg_pipeline pip;
    } msaa;
    struct {
        sg_pipeline pip;
        sg_sampler smp;
        sg_pass_action pass_action;
    } display;
} state;

static void print_webgl2_note(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();
    sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_oric(), .logger.func = slog_func });

    // setup display pass resources (renders the offscreen render target textures
    // as 'fullscreen triangles')
    state.display.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "sampler",
    });
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(fullscreen_shader_desc(sg_query_backend())),
        .label = "display-pipeline",
    });
    state.display.pass_action = (sg_pass_action){
        .colors[0].load_action = SG_LOADACTION_DONTCARE,
    };

    // a common shader to render a bufferless triangle, used
    // both in the srgb-nomsaa and srgb-msaa offscreen render passes
    sg_shader triangle_shader = sg_make_shader(triangle_shader_desc(sg_query_backend()));

    // setup no-msaa pass resources
    state.nomsaa.img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_SRGB8A8, // NOTE: SRGB pixel format
        .label = "nomsaa-image",
    });
    state.nomsaa.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = state.nomsaa.img,
        .label = "nomsaa-tex-view",
    });
    state.nomsaa.pass = (sg_pass) {
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.05f, 0.025f, 1.0f },
        },
        .attachments.colors[0] = sg_make_view(&(sg_view_desc){
            .color_attachment.image = state.nomsaa.img,
            .label = "nomsaa-att-view",
        }),
        .label = "nomsaa-pass",
    };
    state.nomsaa.pip = sg_make_pipeline(&(sg_pipeline_desc){
        // NOTE: default-state is fine for rendering a 2D-triangle
        .shader = triangle_shader,
        .colors[0].pixel_format = SG_PIXELFORMAT_SRGB8A8,
        .label = "nomsaa-pipeline",
    });

    // setup msaa-pass resources
    state.msaa.msaa_img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_SRGB8A8, // NOTE: SRGB pixel format
        .sample_count = MSAA_SAMPLE_COUNT,      // NOTE: with MSAA
        .label = "msaa-image",
    });
    state.msaa.resolve_img = sg_make_image(&(sg_image_desc){
        .usage.resolve_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .sample_count = 1,
        .pixel_format = SG_PIXELFORMAT_SRGB8A8, // NOTE: same image format as render image
        .label = "msaa-resolve-image",
    });
    state.msaa.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = state.msaa.resolve_img,
        .label = "msaa-tex-view"
    });
    state.msaa.pass = (sg_pass){
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .store_action = SG_STOREACTION_DONTCARE,
            .clear_value = { 0.0f, 0.025f, 0.05f, 1.0f },
        },
        .attachments = {
            .colors[0] = sg_make_view(&(sg_view_desc){
                .color_attachment.image = state.msaa.msaa_img,
            }),
            .resolves[0] = sg_make_view(&(sg_view_desc){
                .resolve_attachment.image = state.msaa.resolve_img,
            }),
        },
        .label = "msaa-pass",
    };
    state.msaa.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = triangle_shader,
        .sample_count = MSAA_SAMPLE_COUNT,
        .colors[0].pixel_format = SG_PIXELFORMAT_SRGB8A8,
        .label = "msaa-pipeline",
    });
}

static void frame(void) {
    print_webgl2_note();

    // render nomsaa offscreen pass
    sg_begin_pass(&state.nomsaa.pass);
    sg_apply_pipeline(state.nomsaa.pip);
    sg_draw(0, 3, 1);
    sg_end_pass();

    // render msaa offscreen pass
    sg_begin_pass(&state.msaa.pass);
    sg_apply_pipeline(state.msaa.pip);
    sg_draw(0, 3, 1);
    sg_end_pass();

    // render display pass (the two offscreen-rendered triangles side-by-sideA)
    const int w = sapp_width();
    const int wh = w / 2;
    const int h = sapp_height();
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    // left triangle (no-msaa)
    sg_apply_viewport(0, 0, wh, h, true);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_tex] = state.nomsaa.tex_view,
        .samplers[SMP_smp] = state.display.smp,
    });
    sg_draw(0, 3, 1);
    // right triangle (msaa)
    sg_apply_viewport(wh, 0, wh + 1, h, true);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_tex] = state.msaa.tex_view,
        .samplers[SMP_smp] = state.display.smp,
    });
    sg_draw(0, 3, 1);
    sg_apply_viewport(0, 0, w, h, true);
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

static void print_webgl2_note(void) {
    #if defined(__EMSCRIPTEN__)
    if (sg_query_backend() == SG_BACKEND_GLES3) {
        sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
        sdtx_origin(0.5f, 2.0f);
        sdtx_puts("NOTE: SRGB framebuffers not supported on WebGL2");
    }
    #endif
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .srgb = true,
        .window_title = "srgb-offscreen-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
