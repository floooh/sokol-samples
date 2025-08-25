//------------------------------------------------------------------------------
//  shapes-sapp.c
//  Simple sokol_shape.h demo.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#define VECMATH_GENERICS
#include "vecmath.h"
#include "dbgui/dbgui.h"
#include "shapes-sapp.glsl.h"

typedef struct {
    vec3_t pos;
    sshape_element_range_t draw;
} shape_t;

enum {
    BOX = 0,
    PLANE,
    SPHERE,
    CYLINDER,
    TORUS,
    NUM_SHAPES
};

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_buffer vbuf;
    sg_buffer ibuf;
    shape_t shapes[NUM_SHAPES];
    vs_params_t vs_params;
    float rx, ry;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sdtx_setup(&(sdtx_desc_t) {
        .fonts[0] = sdtx_font_oric(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // clear to black
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // shader and pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(shapes_shader_desc(sg_query_backend())),
        .layout = {
            .buffers[0] = sshape_vertex_buffer_layout_state(),
            .attrs = {
                [0] = sshape_position_vertex_attr_state(),
                [1] = sshape_normal_vertex_attr_state(),
                [2] = sshape_texcoord_vertex_attr_state(),
                [3] = sshape_color_vertex_attr_state()
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
    });

    // shape positions
    state.shapes[BOX].pos = vec3(-1.0f, 1.0f, 0.0f);
    state.shapes[PLANE].pos = vec3(1.0f, 1.0f, 0.0f);
    state.shapes[SPHERE].pos = vec3(-2.0f, -1.0f, 0.0f);
    state.shapes[CYLINDER].pos = vec3(2.0f, -1.0f, 0.0f);
    state.shapes[TORUS].pos = vec3(0.0f, -1.0f, 0.0f);

    // generate shape geometries
    sshape_vertex_t vertices[6 * 1024];
    uint16_t indices[16 * 1024];
    sshape_buffer_t buf = {
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer  = SSHAPE_RANGE(indices),
    };
    buf = sshape_build_box(&buf, &(sshape_box_t){
        .width  = 1.0f,
        .height = 1.0f,
        .depth  = 1.0f,
        .tiles  = 10,
        .random_colors = true,
    });
    state.shapes[BOX].draw = sshape_element_range(&buf);
    buf = sshape_build_plane(&buf, &(sshape_plane_t){
        .width = 1.0f,
        .depth = 1.0f,
        .tiles = 10,
        .random_colors = true,
    });
    state.shapes[PLANE].draw = sshape_element_range(&buf);
    buf = sshape_build_sphere(&buf, &(sshape_sphere_t) {
        .radius = 0.75f,
        .slices = 36,
        .stacks = 20,
        .random_colors = true,
    });
    state.shapes[SPHERE].draw = sshape_element_range(&buf);
    buf = sshape_build_cylinder(&buf, &(sshape_cylinder_t) {
        .radius = 0.5f,
        .height = 1.5f,
        .slices = 36,
        .stacks = 10,
        .random_colors = true,
    });
    state.shapes[CYLINDER].draw = sshape_element_range(&buf);
    buf = sshape_build_torus(&buf, &(sshape_torus_t) {
        .radius = 0.5f,
        .ring_radius = 0.3f,
        .rings = 36,
        .sides = 18,
        .random_colors = true,
    });
    state.shapes[TORUS].draw = sshape_element_range(&buf);
    assert(buf.valid);

    // one vertex/index-buffer-pair for all shapes
    const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
    const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
    state.vbuf = sg_make_buffer(&vbuf_desc);
    state.ibuf = sg_make_buffer(&ibuf_desc);
}

static void frame(void) {
    // help text
    sdtx_canvas(sapp_width()*0.5f, sapp_height()*0.5f);
    sdtx_pos(0.5f, 0.5f);
    sdtx_puts("press key to switch draw mode:\n\n"
              "  1: vertex normals\n"
              "  2: texture coords\n"
              "  3: vertex color");

    // view-projection matrix...
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), sapp_widthf()/sapp_heightf(), 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);

    // model-rotation matrix
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t;
    state.ry += 2.0f * t;
    const mat44_t rxm = mat44_rotation_x(vm_radians(state.rx));
    const mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    const mat44_t rm = vm_mul(rym, rxm);

    // render shapes...
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf
    });
    for (int i = 0; i < NUM_SHAPES; i++) {
        // per shape model-view-projection matrix
        const mat44_t model = vm_mul(rm, mat44_translation(state.shapes[i].pos.x, state.shapes[i].pos.y, state.shapes[i].pos.z));
        state.vs_params.mvp = vm_mul(model, view_proj);
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(state.vs_params));
        sg_draw(state.shapes[i].draw.base_element, state.shapes[i].draw.num_elements, 1);
    }
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        switch (ev->key_code) {
            case SAPP_KEYCODE_1: state.vs_params.draw_mode = 0.0f; break;
            case SAPP_KEYCODE_2: state.vs_params.draw_mode = 1.0f; break;
            case SAPP_KEYCODE_3: state.vs_params.draw_mode = 2.0f; break;
            default: break;
        }
    }
    __dbgui_event(ev);
}

static void cleanup(void) {
    __dbgui_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "shapes-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
