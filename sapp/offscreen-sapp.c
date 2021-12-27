//------------------------------------------------------------------------------
//  offscreen-sapp.c
//  Render to an offscreen rendertarget texture, and use this texture
//  for rendering to the display.
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
#include "offscreen-sapp.glsl.h"

#define OFFSCREEN_SAMPLE_COUNT (4)

static struct {
    struct {
        sg_pass_action pass_action;
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } deflt;
    sshape_element_range_t donut;
    sshape_element_range_t sphere;
    float rx, ry;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    /* default pass action: clear to blue-ish */
    state.deflt.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.45f, 0.65f, 1.0f } }
    };

    /* offscreen pass action */
    state.offscreen.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.25f, 0.25f, 1.0f } }
    };

    /* a render pass with one color- and one depth-attachment image */
    sg_image_desc img_desc = {
        .render_target = true,
        .width = 256,
        .height = 256,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "color-image"
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    img_desc.label = "depth-image";
    sg_image depth_img = sg_make_image(&img_desc);
    state.offscreen.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = color_img,
        .depth_stencil_attachment.image = depth_img,
        .label = "offscreen-pass"
    });

    /* a donut shape which is rendered into the offscreen render target, and
       a sphere shape which is rendered into the default framebuffer
    */
    sshape_vertex_t vertices[4000] = { 0 };
    uint16_t indices[24000] = { 0 };
    sshape_buffer_t buf = {
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer  = SSHAPE_RANGE(indices),
    };
    buf = sshape_build_torus(&buf, &(sshape_torus_t){
        .radius = 0.5f,
        .ring_radius = 0.3f,
        .sides = 20,
        .rings = 36,
    });
    state.donut = sshape_element_range(&buf);
    buf = sshape_build_sphere(&buf, &(sshape_sphere_t) {
        .radius = 0.5f,
        .slices = 72,
        .stacks = 40
    });
    state.sphere = sshape_element_range(&buf);

    const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
    const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

    /* pipeline-state-object for offscreen-rendered donut, don't need texture coord here */
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0] = sshape_buffer_layout_desc(),
            .attrs = {
                [ATTR_vs_offscreen_position] = sshape_position_attr_desc(),
                [ATTR_vs_offscreen_normal] = sshape_normal_attr_desc()
            }
        },
        .shader = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "offscreen-pipeline"
    });

    /* and another pipeline-state-object for the default pass */
    state.deflt.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0] = sshape_buffer_layout_desc(),
            .attrs = {
                [ATTR_vs_default_position] = sshape_position_attr_desc(),
                [ATTR_vs_default_normal] = sshape_normal_attr_desc(),
                [ATTR_vs_default_texcoord0] = sshape_texcoord_attr_desc()
            }
        },
        .shader = sg_make_shader(default_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .label = "default-pipeline"
    });

    /* the resource bindings for rendering a non-textured cube into offscreen render target */
    state.offscreen.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* resource bindings to render a textured cube, using the offscreen render target as texture */
    state.deflt.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[SLOT_tex] = color_img
    };
}

/* helper function to compute model-view-projection matrix */
static hmm_mat4 compute_mvp(float rx, float ry, float aspect, float eye_dist) {
    hmm_mat4 proj = HMM_Perspective(45.0f, aspect, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, eye_dist), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rym, rxm);
    hmm_mat4 mvp = HMM_MultiplyMat4(view_proj, model);
    return mvp;
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t;
    state.ry += 2.0f * t;
    vs_params_t vs_params;

    /* the offscreen pass, rendering an rotating, untextured donut into a render target image */
    vs_params = (vs_params_t) {
        .mvp = compute_mvp(state.rx, state.ry, 1.0f, 2.5f)
    };
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(state.donut.base_element, state.donut.num_elements, 1);
    sg_end_pass();

    /* and the display-pass, rendering a rotating textured sphere which uses the
       previously rendered offscreen render-target as texture
    */
    int w = sapp_width();
    int h = sapp_height();
    vs_params = (vs_params_t) {
        .mvp = compute_mvp(-state.rx * 0.25f, state.ry * 0.25f, (float)w/(float)h, 2.0f)
    };
    sg_begin_default_pass(&state.deflt.pass_action, w, h);
    sg_apply_pipeline(state.deflt.pip);
    sg_apply_bindings(&state.deflt.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(state.sphere.base_element, state.sphere.num_elements, 1);
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
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "Offscreen Rendering (sokol-app)",
        .icon.sokol_default = true,
    };
}
