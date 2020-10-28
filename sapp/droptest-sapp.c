//------------------------------------------------------------------------------
//  droptest-sapp.c
//  Test drag'n'drop file loading.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    simgui_setup(&(simgui_desc_t){ 0 });
}

static void frame(void) {
    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame(width, height, 1.0/60.0);

    igSetNextWindowSize((ImVec2){200,100}, ImGuiCond_Once);
    igBegin("Hello World", 0, 0);
    igText("Bla");
    igEnd();

    sg_begin_default_pass(&(sg_pass_action){0}, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "droptest-sapp",
        .enable_dragndrop = true,
        .max_dropped_files = 4,
    };
}
