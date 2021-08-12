//------------------------------------------------------------------------------
//  sgl-context-sapp.c
//
//  Render into different passes/rendertargets with sokol-gl contexts.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "dbgui/dbgui.h"

static struct {
    struct {
        sg_pass_action pass_action;
        sg_pass pass;
        sgl_context sgl_ctx;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sgl_pipeline sgl_pip;
    } display;
} state;

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-gl with the default context compatible with the default render pass
    sgl_setup(&(sgl_desc_t){
        .context = {
            .max_vertices = 1024,
            .max_command = 128,
        }
    });

    // pass action and pipeline for the default render pass
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.5f, 1.0f, 1.0f } }
    };
    state.display.sgl_pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL
        }
    });

    // create a sokol-gl context compatible with the offscreen render pass (no MSAA)
    state.offscreen.sgl_ctx = sgl_make_context(&(sgl_context_desc_t){
        .max_vertices = 1024,
        .max_commands = 128,
        .sample_count = 1,
    });


}

static void frame(void) {

}

static void cleanup(void) {

}


sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "sokol-gl contexts (sapp)",
        .icon.sokol_default = true,
    };
}
