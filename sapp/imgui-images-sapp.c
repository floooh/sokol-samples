//------------------------------------------------------------------------------
//  imgui-images-sapp.c
//
//  Demonstrates how to use sokol-gfx images and sampler with Dear ImGui
//  via sokol_imgui.h
//
//  Uses sokol-gl to render into a render target texture, and then
//  render the render target texture with different samplers in Dear ImGui.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

#define OFFSCREEN_WIDTH (32)
#define OFFSCREEN_HEIGHT (32)
#define OFFSCREEN_COLOR_FORMAT (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_DEPTH_FORMAT (SG_PIXELFORMAT_DEPTH)
#define OFFSCREEN_SAMPLE_COUNT (1)

static struct {
    double angle_deg;
    struct {
        sgl_context sgl_ctx;
        sgl_pipeline sgl_pip;
        sg_view tex_view;
        sg_pass pass;
    } offscreen;
    struct {
        sg_pass_action pass_action;
    } display;
    struct {
        sg_sampler nearest_clamp;
        sg_sampler linear_clamp;
        sg_sampler nearest_repeat;
        sg_sampler linear_mirror;
    } smp;
} state;

static void draw_cube(void);

// helper function to construct ImTextureRef from ImTextureID
// FIXME: remove when Dear Bindings offers such helper
static ImTextureRef imtexref(ImTextureID tex_id) {
    return (ImTextureRef){ ._TexID = tex_id };
}


static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func,
    });

    // a sokol-gl context to render into an offscreen pass
    state.offscreen.sgl_ctx = sgl_make_context(&(sgl_context_desc_t){
        .color_format = OFFSCREEN_COLOR_FORMAT,
        .depth_format = OFFSCREEN_DEPTH_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // a sokol-gl pipeline object for 3D rendering
    state.offscreen.sgl_pip = sgl_context_make_pipeline(state.offscreen.sgl_ctx, &(sg_pipeline_desc){
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .pixel_format = OFFSCREEN_DEPTH_FORMAT,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .colors[0].pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // a color- and depth- render target image
    sg_image color_img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });
    sg_image depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_DEPTH_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // texture- and attachment-views
    state.offscreen.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = color_img,
    });
    sg_view color_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = color_img,
    });
    sg_view depth_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment.image = depth_img,
    });

    // the offscreen render pass struct
    state.offscreen.pass = (sg_pass){
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f },
            },
        },
        .attachments = {
            .colors[0] = color_view,
            .depth_stencil = depth_view,
        },
    };

    // a pass action for the default pass which clears to blue-ish
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f } },
    };

    // create various samplers which we'll use later during UI rendering
    state.smp.nearest_clamp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });
    state.smp.linear_clamp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });
    state.smp.nearest_repeat = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
    });
    state.smp.linear_mirror = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_MIRRORED_REPEAT,
        .wrap_v = SG_WRAP_MIRRORED_REPEAT,
    });
}

static void frame(void) {
    state.angle_deg += sapp_frame_duration() * 60.0;
    const float a = sgl_rad((float)state.angle_deg);

    // first use sokol-gl to render a rotating cube into the sokol-gl context,
    // this just records draw command for the later sokol-gfx render pass
    sgl_set_context(state.offscreen.sgl_ctx);
    sgl_defaults();
    sgl_load_pipeline(state.offscreen.sgl_pip);
    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(45.0f), 1.0f, 0.1f, 100.0f);
    const float eye[3] = { sinf(a) * 4.0f, sinf(a) * 2.0f, cosf(a) * 4.0f };
    sgl_matrix_mode_modelview();
    sgl_lookat(eye[0], eye[1], eye[2], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    draw_cube();

    // specify the Dear ImGui UI (this also just records commands which are
    // then rendered later in the frame in the sokol-gfx default pass)
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    igSetNextWindowPos((ImVec2){20, 20}, ImGuiCond_Once);
    igSetNextWindowSize((ImVec2){540, 560}, ImGuiCond_Once);
    if (igBegin("Sokol + Dear ImGui Image Test", 0, 0)) {
        const ImVec2 size = { 256, 256 };
        const ImVec2 uv0 = { 0, 0 };
        const ImVec2 uv1 = { 1, 1 };
        const ImVec2 uv2 = { 4, 4 };
        sg_view view = state.offscreen.tex_view;
        ImTextureID texid0 = simgui_imtextureid_with_sampler(view, state.smp.nearest_clamp);
        ImTextureID texid1 = simgui_imtextureid_with_sampler(view, state.smp.linear_clamp);
        ImTextureID texid2 = simgui_imtextureid_with_sampler(view, state.smp.nearest_repeat);
        ImTextureID texid3 = simgui_imtextureid_with_sampler(view, state.smp.linear_mirror);
        igImageEx(imtexref(texid0), size, uv0, uv1); igSameLineEx(0, 4);
        igImageEx(imtexref(texid1), size, uv0, uv1);
        igImageEx(imtexref(texid2), size, uv0, uv2); igSameLineEx(0, 4);
        igImageEx(imtexref(texid3), size, uv0, uv2);
    }
    igEnd();

    // perform sokol-gfx rendering...
    // ...first the offscreen pass which renders the sokol-gl scene
    sg_begin_pass(&state.offscreen.pass);
    sgl_context_draw(state.offscreen.sgl_ctx);
    sg_end_pass();

    // then the display pass with the Dear ImGui scene
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain()
    });
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sgl_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .width = 580,
        .height = 600,
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .window_title = "imgui-images-sapp",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}

// vertex specification for a cube with colored sides and texture coords
static void draw_cube(void) {
    sgl_begin_quads();
    sgl_c3f(1.0f, 0.0f, 0.0f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f, -1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f, -1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(0.0f, 1.0f, 0.0f);
        sgl_v3f_t2f(-1.0f, -1.0f,  1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f,  1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f,  1.0f, -1.0f, -1.0f);
    sgl_c3f(0.0f, 0.0f, 1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f,  1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.5f, 0.0f);
        sgl_v3f_t2f(1.0f, -1.0f,  1.0f, -1.0f,   1.0f);
        sgl_v3f_t2f(1.0f, -1.0f, -1.0f,  1.0f,   1.0f);
        sgl_v3f_t2f(1.0f,  1.0f, -1.0f,  1.0f,  -1.0f);
        sgl_v3f_t2f(1.0f,  1.0f,  1.0f, -1.0f,  -1.0f);
    sgl_c3f(0.0f, 0.5f, 1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f,  1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.0f, 0.5f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0f,  1.0f,  1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f,  1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f, -1.0f, -1.0f, -1.0f);
    sgl_end();
}
