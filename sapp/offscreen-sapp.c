//------------------------------------------------------------------------------
//  offscreen-sapp.c
//  Render to a offscreen rendertarget texture without multisampling, and
//  use this texture for rendering to the display (with multisampling).
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "dbgui/dbgui.h"
#include "offscreen-sapp.glsl.h"

#define OFFSCREEN_PIXEL_FORMAT (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_SAMPLE_COUNT (1)
#define DISPLAY_SAMPLE_COUNT (4)

static struct {
    struct {
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } display;
    sshape_element_range_t donut;
    sshape_element_range_t sphere;
    float rx, ry;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // default pass action: clear to blue-ish
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.45f, 0.65f, 1.0f }
        }
    };

    // setup a render pass struct with one color and one depth render attachment image
    // NOTE: we need to explicitly set the sample count in the attachment image objects,
    // because the offscreen pass uses a different sample count than the display render pass
    // (the display render pass is multi-sampled, the offscreen pass is not)
    sg_image_desc img_desc = {
        .usage = { .color_attachment = true },
        .width = 256,
        .height = 256,
        .pixel_format = OFFSCREEN_PIXEL_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "color-image"
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    img_desc.usage = (sg_image_usage){ .depth_stencil_attachment = true },
    img_desc.label = "depth-image";
    sg_image depth_img = sg_make_image(&img_desc);

    // setup a pass struct with attachment views and pass-actions
    state.offscreen.pass = (sg_pass) {
        .attachments = {
            .colors[0] = sg_make_view(&(sg_view_desc){
                .color_attachment = { .image = color_img },
                .label = "color-attachment",
            }),
            .depth_stencil = sg_make_view(&(sg_view_desc){
                .depth_stencil_attachment = { .image = depth_img },
                .label = "depth-attachment",
            }),
        },
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.25f, 0.25f, 0.25f, 1.0f }
            }
        },
        .label = "offscreen-pass"
    };

    // a donut shape which is rendered into the offscreen render target, and
    // a sphere shape which is rendered into the default framebuffer
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

    sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
    sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
    vbuf_desc.label = "shape-vbuf";
    ibuf_desc.label = "shape-ibuf";
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

    // pipeline-state-object for offscreen-rendered donut
    // NOTE: we need to explicitly set the sample_count here because
    // the offscreen pass uses a different sample count than the default
    // pass (the display pass is multi-sampled, but the offscreen pass isn't)
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0] = sshape_vertex_buffer_layout_state(),
            .attrs = {
                [ATTR_offscreen_position] = sshape_position_vertex_attr_state(),
                [ATTR_offscreen_normal] = sshape_normal_vertex_attr_state()
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
        .colors[0].pixel_format = OFFSCREEN_PIXEL_FORMAT,
        .label = "offscreen-pipeline"
    });

    // and another pipeline-state-object for the default pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0] = sshape_vertex_buffer_layout_state(),
            .attrs = {
                [ATTR_default_position] = sshape_position_vertex_attr_state(),
                [ATTR_default_normal] = sshape_normal_vertex_attr_state(),
                [ATTR_default_texcoord0] = sshape_texcoord_vertex_attr_state()
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

    // a sampler object for sampling the render target texture
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .label = "sampler",
    });

    // the resource bindings for rendering a non-textured shape into offscreen render target
    state.offscreen.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    // resource bindings to render a textured shape, using the offscreen render target as texture
    state.display.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .views[VIEW_tex] = sg_make_view(&(sg_view_desc){
            .texture = { .image = color_img },
            .label = "texture-view",
        }),
        .samplers[SMP_smp] = smp,
    };
}

// helper function to compute model-view-projection matrix
static mat44_t compute_mvp(float rx, float ry, float aspect, float eye_dist) {
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(45.0f), aspect, 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, eye_dist), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    const mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    const mat44_t rym = mat44_rotation_y(vm_radians(ry));
    const mat44_t model = vm_mul(rxm, rym);
    const mat44_t mvp = vm_mul(model, view_proj);
    return mvp;
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t;
    state.ry += 2.0f * t;
    vs_params_t vs_params;

    // the offscreen pass, rendering an rotating, untextured donut into a render target image
    vs_params = (vs_params_t) {
        .mvp = compute_mvp(state.rx, state.ry, 1.0f, 2.5f)
    };
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(state.donut.base_element, state.donut.num_elements, 1);
    sg_end_pass();

    // and the display-pass, rendering a rotating textured sphere which uses the
    // previously rendered offscreen render-target as texture
    int w = sapp_width();
    int h = sapp_height();
    vs_params = (vs_params_t) {
        .mvp = compute_mvp(-state.rx * 0.25f, state.ry * 0.25f, (float)w/(float)h, 1.5f)
    };
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain(),
        .label = "swapchain-pass",
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
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
        .sample_count = DISPLAY_SAMPLE_COUNT,
        .window_title = "offscreen-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
