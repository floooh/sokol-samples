//------------------------------------------------------------------------------
//  draw-indirect-sapp.c
//
//  Demonstrates the draw-indirect features (indirect-buffers and
//  sg_draw_indirect() call).
//
//  - shape instance positions are updated in a compute shader
//  - another compute shader clips against a view volume and updates
//    two parameter set for instanced rendering in an indirect buffer
//    (one set for all 'inside' shapes, another for all 'outside' shapes)
//  - finally render the shapes via sg_draw_indirect().
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"
#define SOKOL_APP_IMGUI_IMPL
#include "sokol_app_imgui.h"
#include "draw-indirect-sapp.glsl.h"

#define MAX_ITEMS (1024 * 1024)
#define WORKGROUP_SIZE (64)

static struct {
    int num_shapes;
    struct {
        sg_buffer buf;
        sg_view view;
    } items;
    struct {
        sg_buffer buf;
        sg_view view;
    } inside;
    struct {
        sg_buffer buf;
        sg_view view;
    } outside;
    struct {
        sg_buffer buf;
        sg_view view;
    } indirect;
    struct {
        sg_pipeline init_pip;
        sg_pipeline update_pip;
        sg_pipeline clip_pip;
    } compute;
    struct {
        sg_pass_action pass_action;
    } display;
} state;

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

    // create a storage buffer with position, velocity and orientation for
    state.items.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .size = MAX_ITEMS * sizeof(item_props_t),
        .label = "item-buffer",
    });
    state.items.view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.items.buf,
        .label = "item-buffer-view",
    });

    // two buffers with indices into the item-buffer, one for the 'inside set'
    // the other for the 'outside set'
    state.inside.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .size = MAX_ITEMS * sizeof(uint32_t),
        .label = "inside-buffer",
    });
    state.inside.view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.inside.buf,
        .label = "inside-buffer-view",
    });
    state.outside.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .size = MAX_ITEMS * sizeof(uint32_t),
        .label = "outside-buffer",
    });
    state.outside.view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.outside.buf,
        .label = "outside-buffer-view",
    });

    // and finally an indirect-buffer with two parameter sets:
    // one for the 'inside' and one for the 'outside' items,
    // indexed rendering requires 6 uint32_t per parameters set
    state.indirect.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .storage_buffer = true,
            .indirect_buffer = true,
        },
        .size = 2 * sizeof(sg_draw_indirect_indexed_args),
        .label = "indirect-buffer",
    });
    state.indirect.view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.indirect.buf,
        .label = "indirect-buffer-view",
    });

    // a pipeline to initialize the item buffer content
    state.compute.init_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(cs_init_shader_desc(sg_query_backend())),
        .label = "init-pipeline",
    });

    state.display.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.102f, 0.137f, 0.494f, 1.0f },
        },
    };

    // run an initialization pass to populate the items-buffer
    // with random positions and velocities, this runs as part
    // of the first frame
    sg_begin_pass(&(sg_pass){ .compute = true });
    sg_apply_pipeline(state.compute.init_pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_cs_ssbo] = state.items.view,
    });
    sg_dispatch(MAX_ITEMS / WORKGROUP_SIZE, 1, 1);
    sg_end_pass();
}

static void frame(void) {
    draw_ui();

    simgui_flush();
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    simgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
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
    sappimgui_draw();
    sgimgui_draw();

    // FIXME: controls window
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "draw-indirect-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
