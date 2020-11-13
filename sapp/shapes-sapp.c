//------------------------------------------------------------------------------
//  shapes-sapp.c
//  Simple sokol_shape.h demo.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "shapes-sapp.glsl.h"

typedef struct {
    hmm_vec3 pos;
    sshape_element_range_t draw;
} shape_t;

enum {
    BOX = 0,
    PLANE,
    SPHERE,
    CYLINDER,
    NUM_SHAPES
};

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_buffer vbuf;
    sg_buffer ibuf;
    shape_t shapes[NUM_SHAPES];
    float rx, ry;
    uint32_t frame_count;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    // clear to black
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // shader and pipeline object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(shapes_shader_desc()),
        .layout = {
            .buffers[0] = sshape_buffer_layout_desc(),
            .attrs = {
                [0] = sshape_position_attr_desc(),
                [1] = sshape_normal_attr_desc(),
                [2] = sshape_texcoord_attr_desc(),
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer.cull_mode = SG_CULLMODE_NONE,
    });

    // shape positions
    state.shapes[BOX].pos = HMM_Vec3(-1.0f, 1.0f, 0.0f);
    state.shapes[PLANE].pos = HMM_Vec3(1.0f, 1.0f, 0.0f);
    state.shapes[SPHERE].pos = HMM_Vec3(-2.0f, -1.0f, 0.0f);
    state.shapes[CYLINDER].pos = HMM_Vec3(2.0f, -1.0f, 0.0f);

    // generate shape geometries
    sshape_vertex_t vertices[2048];
    uint16_t indices[8192];
    sshape_build_t build = {
        .vertices = { .buffer_ptr = vertices, .buffer_size = sizeof(vertices) },
        .indices = { .buffer_ptr = indices, .buffer_size = sizeof(indices) }
    };
    build = sshape_build_box(&build, &(sshape_box_t){
        .width=1.0f,
        .height=1.0f,
        .depth=1.0f
    });
    state.shapes[BOX].draw = sshape_element_range(&build);
    build = sshape_build_plane(&build, &(sshape_plane_t){
        .width=1.0f,
        .depth=1.0f
    });
    state.shapes[PLANE].draw = sshape_element_range(&build);
    build = sshape_build_sphere(&build, &(sshape_sphere_t) {
        .radius = 0.75f,
        .slices = 36,
        .stacks = 20
    });
    state.shapes[SPHERE].draw = sshape_element_range(&build);
    build = sshape_build_cylinder(&build, &(sshape_cylinder_t) {
        .radius = 0.5f,
        .height = 1.5f,
        .slices = 36,
        .stacks = 10,
    });
    state.shapes[CYLINDER].draw = sshape_element_range(&build);
    assert(build.valid);

    // one vertex/index-buffer-pair for all shapes
    const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&build);
    const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&build);
    state.vbuf = sg_make_buffer(&vbuf_desc);
    state.ibuf = sg_make_buffer(&ibuf_desc);
}

static void frame(void) {
    state.frame_count++;

    // view-projection matrix...
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // model-rotation matrix
    state.rx += 1.0f;
    state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 rm = HMM_MultiplyMat4(rxm, rym);

    // render shapes...
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf
    });
    for (int i = 0; i < NUM_SHAPES; i++) {
        // per shape model-view-projection matrix
        hmm_mat4 model = HMM_MultiplyMat4(HMM_Translate(state.shapes[i].pos), rm);
        vs_params_t vs_params = {
            .mvp = HMM_MultiplyMat4(view_proj, model),
            .draw_normals = state.frame_count & 0x40 ? 0.0f : 1.0f,
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &vs_params, sizeof(vs_params));
        sg_draw(state.shapes[i].draw.base_element, state.shapes[i].draw.num_elements, 1);
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}


static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "sokol_shapes.h Demo"
    };
}
