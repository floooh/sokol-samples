//------------------------------------------------------------------------------
//  framebuffer-sapp.c
//  Test/demonstrate sokol_framebuffer.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#define SOKOL_FRAMEBUFFER_IMPL
#include "sokol_framebuffer.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#define SOKOL_APP_IMGUI_IMPL
#include "sokol_app_imgui.h"

#define FB_WIDTH (320)
#define FB_HEIGHT (200)

static struct {
    sg_pass_action pass_action;
    sfb_framebuffer fb;
} state;

static uint32_t pixels[FB_WIDTH][FB_HEIGHT];

static void draw_ui(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgimgui_setup(&(sgimgui_desc_t){0});
    sappimgui_setup();
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });
    sfb_setup(&(sfb_desc){
        .logger.func = slog_func,
    });

    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 1.0f, 1.0f }
        },
    };

    // FIXME: use paletted format
    state.fb = sfb_make_framebuffer(&(sfb_framebuffer_desc){
        .width = FB_WIDTH,
        .height = FB_HEIGHT,
        .prescale = 2,
    });
}

static void frame(void) {
    draw_ui();

    // update framebuffer
    static size_t c = 0;
    for (size_t y = 0; y < FB_HEIGHT; y++) {
        for (size_t x = 0; x < FB_WIDTH; x++) {
            pixels[x][y] = 0xFF000000 | (c & 255)<<16 | ((c*3) & 255)<<8 | ((c*23) & 0xFF);
            c += 1;
        }
    }
    c += 1;

    // run sfb_update() outside any sokol-gfx pass when pixel, palette or cliprect changes
    sfb_update(state.fb, &(sfb_update_desc){
        .pixels = SG_RANGE(pixels),
    });

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sfb_render(state.fb);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sfb_shutdown();
    sappimgui_shutdown();
    sgimgui_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    sappimgui_track_event(ev);
    simgui_handle_event(ev);
}

static void draw_ui(void) {
    sappimgui_track_frame();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });
    if (igBeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        sappimgui_draw_menu("sokol-app");
        igEndMainMenuBar();
    }
    sgimgui_draw();
    sappimgui_draw();
    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        igText("FIXME");
    }
    igEnd();
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
        .window_title = "framebuffer-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
