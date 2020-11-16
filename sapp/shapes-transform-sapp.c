//------------------------------------------------------------------------------
//  shapes-transform-sapp.c
//
//  Demonstrates merging multiple transformed shapes into a single draw-shape
//  with sokol_shape.h
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "shapes-transform-sapp.glsl.h"

struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    sshape_element_range_t elms;
    vs_params_t vs_params;
    float rx, ry;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_oric()
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
                [3] = sshape_color_attr_desc()
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer.cull_mode = SG_CULLMODE_NONE,
    });

    // generate merged shape geometries
    sshape_vertex_t vertices[6 * 1024];
    uint16_t indices[16 * 1024];
    sshape_buffer_t buf = {
        .vertices = { .buffer_ptr = vertices, .buffer_size = sizeof(vertices) },
        .indices  = { .buffer_ptr = indices, .buffer_size = sizeof(indices) }
    };

    // transform matrices for the shapes
    const hmm_mat4 box_transform = HMM_Translate(HMM_Vec3(-1.0f, 0.0f, +1.0f));
    const hmm_mat4 sphere_transform = HMM_Translate(HMM_Vec3(+1.0f, 0.0f, +1.0f));
    const hmm_mat4 cylinder_transform = HMM_Translate(HMM_Vec3(-1.0f, 0.0f, -1.0f));
    const hmm_mat4 torus_transform = HMM_Translate(HMM_Vec3(+1.0f, 0.0f, -1.0f));

    // build the shapes...
    buf = sshape_build_box(&buf, &(sshape_box_t){
        .width  = 1.0f,
        .height = 1.0f,
        .depth  = 1.0f,
        .tiles  = 10,
        .random_colors = true,
        .transform = sshape_mat4(&box_transform.Elements[0][0])
    });
    buf = sshape_build_sphere(&buf, &(sshape_sphere_t){
        .merge = true,
        .radius = 0.75f,
        .slices = 36,
        .stacks = 20,
        .random_colors = true,
        .transform = sshape_mat4(&sphere_transform.Elements[0][0])
    });
    buf = sshape_build_cylinder(&buf, &(sshape_cylinder_t) {
        .merge = true,
        .radius = 0.5f,
        .height = 1.0f,
        .slices = 36,
        .stacks = 10,
        .random_colors = true,
        .transform = sshape_mat4(&cylinder_transform.Elements[0][0])
    });
    buf = sshape_build_torus(&buf, &(sshape_torus_t) {
        .merge = true,
        .radius = 0.5f,
        .ring_radius = 0.3f,
        .rings = 36,
        .sides = 18,
        .random_colors = true,
        .transform = sshape_mat4(&torus_transform.Elements[0][0])
    });
    assert(buf.valid);

    // extract element range for sg_draw()
    state.elms = sshape_element_range(&buf);

    // and finally create the vertex- and index-buffer
    const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
    const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
    state.bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
    state.bind.index_buffer = sg_make_buffer(&ibuf_desc);
}

static void frame(void) {
    // help text
    sdtx_canvas(sapp_width()*0.5f, sapp_height()*0.5f);
    sdtx_pos(0.5f, 0.5f);
    sdtx_puts("press key to switch draw mode:\n\n"
              "  1: vertex normals\n"
              "  2: texture coords\n"
              "  3: vertex color");

    // build model-view-projection matrix
    state.rx += 1.0f;
    state.ry += 2.0f;
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    state.vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    // render the single shape
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &state.vs_params, sizeof(state.vs_params));
    sg_draw(state.elms.base_element, state.elms.num_elements, 1);

    // render help text and finish frame
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
        .window_title = "shapes-transform-sapp.c"
    };
}
