//------------------------------------------------------------------------------
//  offscreen-msaa-sapp.c
//  Render to a multisampled offscreen rendertarget texture, resolve into
//  a separate non-multisampled texture, and use this as sampled texture
//  in the display render pass.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "offscreen-msaa-sapp.glsl.h"

#define OFFSCREEN_WIDTH (256)
#define OFFSCREEN_HEIGHT (256)
#define OFFSCREEN_COLOR_FORMAT (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_DEPTH_FORMAT (SG_PIXELFORMAT_DEPTH)
#define OFFSCREEN_SAMPLE_COUNT (4)
#define DISPLAY_SAMPLE_COUNT (4)

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
    } display;
    sshape_element_range_t sphere;
    sshape_element_range_t donut;
    float rx, ry;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // default pass action: clear to green-ish
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.65f, 0.45f, 1.0f }
        },
    };

    // offscreen pass action: clear to grey
    // NOTE: we don't need to store the MSAA render target content, because
    // it will be resolved into a non-MSAA texture at the end of the
    // offscreen pass
    state.offscreen.pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .store_action = SG_STOREACTION_DONTCARE,
            .clear_value = { 0.25f, 0.25f, 0.25f, 1.0f }
        }
    };

    // create a MSAA render target image, this will be rendered to
    // in the offscreen render pass
    const sg_image msaa_image = sg_make_image(&(sg_image_desc){
        .render_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "msaa-image"
    });

    // create a depth-buffer image for the offscreen pass,
    // this needs the same dimensions and sample count as the
    // render target image
    const sg_image depth_image = sg_make_image(&(sg_image_desc){
        .render_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_DEPTH_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "depth-image",
    });

    // create a matching resolve-image where the MSAA-rendered content will
    // be resolved to at the end of the offscreen pass, and which will be
    // texture-sampled in the display pass
    const sg_image resolve_image = sg_make_image(&(sg_image_desc){
        .render_attachment = true,
        .width = OFFSCREEN_WIDTH,
        .height = OFFSCREEN_HEIGHT,
        .pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = 1,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .label = "resolve-image",
    });

    // finally, create the offscreen render pass object, by setting a resolve-attachment,
    // an MSAA-resolve operation will happen from the color attachment into the
    // resolve attachment in sg_end_pass()
    state.offscreen.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = msaa_image,
        .resolve_attachments[0].image = resolve_image,
        .depth_stencil_attachment.image = depth_image,
        .label = "offscreen-pass",
    });

    // create a couple of meshes
    sshape_vertex_t vertices[4000] = { 0 };
    uint16_t indices[24000] = { 0 };
    sshape_buffer_t buf = {
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer  = SSHAPE_RANGE(indices),
    };
    buf = sshape_build_sphere(&buf, &(sshape_sphere_t) {
        .radius = 0.5f,
        .slices = 18,
        .stacks = 10
    });
    state.sphere = sshape_element_range(&buf);
    buf = sshape_build_torus(&buf, &(sshape_torus_t) {
        .radius = 0.3f,
        .ring_radius = 0.2f,
        .sides = 40,
        .rings = 72,
    });
    state.donut = sshape_element_range(&buf);

    sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
    sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
    vbuf_desc.label = "shape-vbuf";
    ibuf_desc.label = "shape-ibuf";
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

    // a pipeline object for the offscreen-rendered box
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0] = sshape_buffer_layout_desc(),
            .attrs = {
                [ATTR_vs_offscreen_position] = sshape_position_attr_desc(),
                [ATTR_vs_offscreen_normal] = sshape_normal_attr_desc(),
            }
        },
        .shader = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .depth = {
            .pixel_format = OFFSCREEN_DEPTH_FORMAT,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = OFFSCREEN_COLOR_FORMAT,
        .label = "offscreen-pipeline",
    });

    // another pipeline object for the display pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0] = sshape_buffer_layout_desc(),
            .attrs = {
                [ATTR_vs_display_position] = sshape_position_attr_desc(),
                [ATTR_vs_display_normal] = sshape_normal_attr_desc(),
                [ATTR_vs_display_texcoord0] = sshape_texcoord_attr_desc(),
            },
        },
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "display-pipeline",
    });

    // the resource bindings for rendering a non-textured shape in the offscreen pass
    state.offscreen.bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
    };

    // the resource bindings for rendering a texture shape in the display pass,
    // using the msaa-resolved image as texture
    state.display.bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[SLOT_tex] = resolve_image,
    };
}

// helper function to compute model-view-projection matrix
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

    // the offscreen pass, rendering an rotating, untextured sphere into an msaa render target image,
    // which is then resolved into a regular non-msaa texture at the end of the pass
    vs_params = (vs_params_t) {
        .mvp = compute_mvp(state.rx, state.ry, 1.0f, 2.5f)
    };
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(state.sphere.base_element, state.sphere.num_elements, 1);
    sg_end_pass();

    // and the display-pass, rendering a rotating textured donut which uses the
    // previously msaa-resolved texture
    int w = sapp_width();
    int h = sapp_height();
    vs_params = (vs_params_t) {
        .mvp = compute_mvp(-state.rx * 0.25f, state.ry * 0.25f, (float)w/(float)h, 2.0f)
    };
    sg_begin_default_pass(&state.display.pass_action, w, h);
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(state.donut.base_element, state.donut.num_elements, 1);
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
        .sample_count = DISPLAY_SAMPLE_COUNT,
        .window_title = "Offscreen MSAA Rendering (sokol-app)",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
