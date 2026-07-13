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

#define SPHERE_RADIUS (1.0f)
#define BOX_SIZE (1.5f)

static struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    struct {
        sshape_element_range_t plane;
        sshape_element_range_t sphere;
        sshape_element_range_t box;
    } shapes;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
    camera_t camera;
    vec3_t light_pos;
    mat44_t light_view_proj;
    mat44_t view_proj;
} state;

static void create_shape_resources(void);
static void update_matrices(void);
static void display_pass_draw_shape(const sshape_element_range_t* shape, mat44_t model, vec3_t color);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    // initialize camera helper
    cam_init(&state.camera, &(camera_desc_t){
        .center = vec3(0.0f, 0.0f, 0.0f),
        .latitude = 25.0f,
        .longitude = 225.0f,
        .distance = 25.0f,
        .max_dist = 300.0f,
    });

    // fixed light position in world space
    state.light_pos = vec3(50.0f, 100.0f, -75.0f);

    state.display.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.2f, 0.4f, 0.8f, 1.0f }
        },
    };


    create_shape_resources();
}

static void frame(void) {
    cam_update(&state.camera, sapp_width(), sapp_height());

    update_matrices();

    // display pass
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    display_pass_draw_shape(&state.shapes.plane, mat44_identity(), vec3(0.5f, 0.5f, 0.5f));
    display_pass_draw_shape(&state.shapes.sphere, mat44_translation(0.0f, 3.0f, 0.0f), vec3(1.0f, 0.0f, 1.0f));
    display_pass_draw_shape(&state.shapes.box, mat44_translation(3.0f, 3.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
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

static void update_matrices(void) {
    // calculate matrices for shadow pass
    const mat44_t light_view = mat44_look_at_rh(state.light_pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t light_proj = mat44_ortho_off_center_rh(-75.0f, 75.0f, -75.0f, 75.0f, 1.0f, 400.0f);
    state.light_view_proj = vm_mul(light_view, light_proj);

    // calculate matrices for display pass
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), sapp_widthf()/sapp_heightf(), 0.1f, 500.0f);
    const mat44_t view = mat44_look_at_rh(state.camera.eye_pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    state.view_proj = vm_mul(view, proj);
}

static void display_pass_draw_shape(const sshape_element_range_t* shape, mat44_t model, vec3_t color) {
    const shape_vs_params_t ground_vs_params = {
        .model = model,
        .mvp = vm_mul(model, state.view_proj),
        .light_mvp = vm_mul(model, state.light_view_proj),
        .diff_color = color,
    };
    const shape_fs_params_t ground_fs_params = {
        .eye_pos = state.camera.eye_pos,
        .light_dir = vm_normalize(state.light_pos),
    };
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
    });
    sg_apply_uniforms(UB_shape_vs_params, &SG_RANGE(ground_vs_params));
    sg_apply_uniforms(UB_shape_fs_params, &SG_RANGE(ground_fs_params));
    sg_draw(shape->base_element, shape->num_elements, 1);
}

static void create_shape_resources(void) {
    uint8_t vertices[SSHAPE_MAX_VERTEX_SIZE * 4096];
    uint16_t indices[4096];
    sshape_state_t shp = {
        .disable = {
            .texcoords = true,
            .colors = true,
        },
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer = SSHAPE_RANGE(indices),
    };
    sshape_build_plane(&shp, &(sshape_plane_t){
        .width = 100.0f,
        .depth = 100.0f,
    });
    state.shapes.plane = sshape_element_range(&shp);
    sshape_build_sphere(&shp, &(sshape_sphere_t){
        .radius = SPHERE_RADIUS,
        .slices = 15,
        .stacks = 11,
    });
    state.shapes.sphere = sshape_element_range(&shp);
    sshape_build_box(&shp, &(sshape_box_t){
        .width = BOX_SIZE,
        .height = BOX_SIZE,
        .depth = BOX_SIZE,
    });
    state.shapes.box = sshape_element_range(&shp);

    const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&shp);
    const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&shp);
    state.vbuf = sg_make_buffer(&vbuf_desc);
    state.ibuf = sg_make_buffer(&ibuf_desc);

    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(shape_shader_desc(sg_query_backend())),
        .layout = {
            .buffers[0] = sshape_vertex_buffer_layout_state(&shp),
            .attrs = {
                [ATTR_shape_position] = sshape_position_vertex_attr_state(&shp),
                [ATTR_shape_normal] = sshape_normal_vertex_attr_state(&shp),
            }
        },
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .label = "shape-pipeline"
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
