//------------------------------------------------------------------------------
//  box3d-simple-sapp.c
//
//  Basic integration with Box3d physics engine.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "box3d/box3d.h"
#include "dbgui/dbgui.h"
#include "util/camera.h"
#include "box3d-simple-sapp.glsl.h"

static struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_buffer inst_buf;
    struct {
        sshape_element_range_t plane;
        sshape_element_range_t cube;
        sshape_element_range_t sphere;
    } shapes;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
    camera_t camera;
} state;

static void create_shape_visuals(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    // initialize camera helper
    cam_init(&state.camera, &(camera_desc_t){
        .latitude = 0.0f,
        .longitude = 45.0f,
        .distance = 25.0f,
    });

    state.display.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.2f, 0.4f, 0.8f, 1.0f }
        },
    };

    create_shape_visuals();

}

static void frame(void) {
    cam_update(&state.camera, sapp_width(), sapp_height());
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    if (__dbgui_event_with_retval(ev)) {
        return;
    }
    cam_handle_event(&state.camera, ev);
}

static void create_shape_visuals(void) {
    sshape_vertex_t vertices[4096];
    uint16_t indices[4096];
    sshape_buffer_t buf = {
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer = SSHAPE_RANGE(indices),
    };
    buf = sshape_build_plane(&buf, &(sshape_plane_t){
        .width = 100.0f,
        .depth = 100.0f,
    });


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
        .sample_count = 4,
        .window_title = "box3d-simple-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
