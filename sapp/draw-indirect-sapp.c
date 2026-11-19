//------------------------------------------------------------------------------
//  draw-indirect-sapp.c
//
//  Demonstrates the draw-indirect features (indirect-buffers and
//  sg_draw_indirect() call).
//
//  Three buffers:
//      (1) to hold position, rotation and velocity for all items
//      (2) to hold indices into (1) for all visible items that survive
//          culling
//      (3) to hold combined storage/indirect buffer to hold a single instanced
//          draw call, the number of instances is the number of visible items
//
//  Three compute shaders:
//      (1) a one-shot shader which initializes buffer (1) with random
//          initial positions, rotations and velocities
//      (2) a per-frame shader which resets the draw-indirect args to
//          zero instances
//      (3) a combined update + cull shader which updates positions and
//          rotations of all items to tumble around within a cube volume,
//          and then to cull items against a dynamic 'gyroid' surface, items
//          that remain visible are appended as index to buffer (2), with
//          the index pointing into buffer (1), and each visible item bumps
//          the num_instances count in the indirect buffer.
//
//  Two per-frame passes:
//      (1) a compute pass for dispatching compute shader (2) and (3)
//      (2) a render pass for rendering the visible items via single
//          sg_draw_indirect() call.
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
#include "util/camera.h"
#include "draw-indirect-sapp.glsl.h"
#include <stdio.h> // snprintf
#include <math.h> // fmodf

#define MAX_ITEMS (1024 * 1024)
#define WORKGROUP_SIZE (64)
#define MAX_WORKGROUPS (MAX_ITEMS / WORKGROUP_SIZE)

static struct {
    int num_workgroups;
    struct {
        float scale;
        float width;
        float speed;
        float phase;
    } cull;
    struct {
        sg_buffer buf;
        sg_view view;
    } items;
    struct {
        sg_buffer buf;
        sg_view view;
    } visible;
    struct {
        sg_buffer buf;
        sg_view view;
    } indirect;
    struct {
        sg_pipeline init_pip;
        sg_pipeline reset_pip;
        sg_pipeline upd_cull_pip;
    } compute;
    struct {
        sg_pass_action pass_action;
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
    } display;
    camera_t camera;
} state;

// vertices and indices for a cube per-face intensities
static const float cube_vertices[] = {
    -1.0f, -1.0f, -1.0f,   0.6f,
     1.0f, -1.0f, -1.0f,   0.6f,
     1.0f,  1.0f, -1.0f,   0.6f,
    -1.0f,  1.0f, -1.0f,   0.6f,

    -1.0f, -1.0f,  1.0f,   1.0f,
     1.0f, -1.0f,  1.0f,   1.0f,
     1.0f,  1.0f,  1.0f,   1.0f,
    -1.0f,  1.0f,  1.0f,   1.0f,

    -1.0f, -1.0f, -1.0f,   0.5f,
    -1.0f,  1.0f, -1.0f,   0.5f,
    -1.0f,  1.0f,  1.0f,   0.5f,
    -1.0f, -1.0f,  1.0f,   0.5f,

     1.0f, -1.0f, -1.0f,   0.8f,
     1.0f,  1.0f, -1.0f,   0.8f,
     1.0f,  1.0f,  1.0f,   0.8f,
     1.0f, -1.0f,  1.0f,   0.8f,

    -1.0f, -1.0f, -1.0f,   0.4f,
    -1.0f, -1.0f,  1.0f,   0.4f,
     1.0f, -1.0f,  1.0f,   0.4f,
     1.0f, -1.0f, -1.0f,   0.4f,

    -1.0f,  1.0f, -1.0f,   0.9f,
    -1.0f,  1.0f,  1.0f,   0.9f,
     1.0f,  1.0f,  1.0f,   0.9f,
     1.0f,  1.0f, -1.0f,   0.9f,
};
static const uint16_t cube_indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

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

    // initialize camera helper
    cam_init(&state.camera, &(camera_desc_t){
        .latitude = -10.0f,
        .longitude = 45.0f,
        .distance = 2.5f,
        .min_dist = 0.25f,
    });

    // initial number of workgroups (each handles 64 items)
    state.num_workgroups = 8192;    // * 64 = 512k items
    state.cull.scale = 3.5f;
    state.cull.width = 0.3f;
    state.cull.speed = 0.5f;

    // a storage buffer and view with the complete item state
    state.items.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .size = MAX_ITEMS * sizeof(item_t),
        .label = "items-buffer",
    });
    state.items.view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.items.buf,
        .label = "items-buffer-view",
    });

    // a storage buffer with just the culling result, these are indices
    // of visible items into the items buffer
    state.visible.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .size = MAX_ITEMS * sizeof(visible_item_t),
        .label = "vis-buffer",
    });
    state.visible.view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.visible.buf,
        .label = "vis-buffer-view",
    });

    // and an indirect buffer and storage-buffer-view for one indirect-argument set
    state.indirect.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .storage_buffer = true,
            .indirect_buffer = true,
        },
        .size = sizeof(draw_args_t),
        .label = "indirect-buffer",
    });
    state.indirect.view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.indirect.buf,
        .label = "indirect-buffer-view",
    });

    // compute pipeline initializing item state (pos, rot, vel)
    state.compute.init_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(init_shader_desc(sg_query_backend())),
        .label = "init-pipeline",
    });
    // compute pipeline for resetting the draw-indirect-args once
    // at start of frame
    state.compute.reset_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(reset_shader_desc(sg_query_backend())),
        .label = "reset-pipeline",
    });
    // compute pipeline for combined update and cull process
    state.compute.upd_cull_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(upd_cull_shader_desc(sg_query_backend())),
        .label = "upd-cull-pipeline",
    });

    // a vertex- and index-buffer for rendering a cube
    state.display.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices),
        .label = "cube-vertex-buffer",
    });
    state.display.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices),
        .label = "cube-index-buffer",
    });

    // a render pipeline to render the moving/tumbling shapes
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_display_in_pos].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_display_in_intensity].format = SG_VERTEXFORMAT_FLOAT,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
        .label = "display-pipeline",
    });

    // pass-action for the display pass (clear to background color)
    state.display.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };

    // run an initialization pass to populate the items-buffer
    // with random positions and velocities, this runs as part
    // of the first frame
    sg_begin_pass(&(sg_pass){ .compute = true });
    sg_apply_pipeline(state.compute.init_pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_cs_items] = state.items.view,
    });
    sg_dispatch(MAX_ITEMS / WORKGROUP_SIZE, 1, 1);
    sg_end_pass();
}

static void frame(void) {
    float dt = (float)sapp_frame_duration();
    const int fb_width = sapp_width();
    const int fb_height = sapp_height();
    cam_update(&state.camera, fb_width, fb_height);

    // move gyroid phase, wrap at 2*PI
    state.cull.phase = fmodf(state.cull.phase + state.cull.speed * dt, 2.0f * VECMATH_PI);
    draw_ui();

    // compute pass: reset draw-indirect params, then
    // update and cull items, and update the draw-indirect-params
    sg_begin_pass(&(sg_pass){ .compute = true });

    // reset the indirect-draw-args
    sg_apply_pipeline(state.compute.reset_pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_cs_indirect] = state.indirect.view,
    });
    sg_dispatch(1, 1, 1);

    // update item state, cull, update draw-indirect params, all in one dispatch
    sg_apply_pipeline(state.compute.upd_cull_pip);
    sg_apply_bindings(&(sg_bindings){
        .views = {
            [VIEW_cs_items] = state.items.view,
            [VIEW_cs_indirect] = state.indirect.view,
            [VIEW_cs_visible] = state.visible.view,
        },
    });
    const cs_upd_cull_params_t cs_upd_cull_params = {
        .dt = dt,
        .scale = state.cull.scale,
        .width = state.cull.width,
        .phase = state.cull.phase,
    };
    sg_apply_uniforms(UB_cs_upd_cull_params, &SG_RANGE(cs_upd_cull_params));
    sg_dispatch(state.num_workgroups, 1, 1);
    sg_end_pass();

    // rendering the visible shapes happens via an indirect-draw-call
    simgui_flush();
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain()
    });
    const vs_params_t vs_params = { .view_proj = state.camera.view_proj };
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.display.vbuf,
        .index_buffer = state.display.ibuf,
        .views = {
            [VIEW_vs_items] = state.items.view,
            [VIEW_vs_visible] = state.visible.view,
        },
    });
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw_indirect(state.indirect.buf, 0);
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
    if (simgui_handle_event(ev)) {
        return;
    }
    cam_handle_event(&state.camera, ev);
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

    igSetNextWindowPos((ImVec2){ 30, 50 }, ImGuiCond_Once);
    igSetNextWindowBgAlpha(0.75f);
    if (igBegin("Controls", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        char label[32];
        snprintf(label, sizeof(label), "%d", state.num_workgroups * WORKGROUP_SIZE);
        igSliderIntEx("Num Cubes", &state.num_workgroups, 1024, MAX_WORKGROUPS, label, ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_NoInput);
        igSliderFloat("Gyroid Scale", &state.cull.scale, 1.0f, 20.0f);
        igSliderFloat("Wall Width", &state.cull.width, 0.01f, 1.5f);
        igSliderFloat("Speed", &state.cull.speed, -2.0f, 2.0f);
    }
    igEnd();
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
        .sample_count = 4,
        .window_title = "draw-indirect-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
