//------------------------------------------------------------------------------
//  cmdbuf-sapp.c
//
//  Same as the offscreen samples, but record the rendering commands
//  outside sokol-gfx render passes into sokol_cmdbuf.h command buffer
//  objects.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define SOKOL_CMDBUF_IMPL
#include "sokol_cmdbuf.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "dbgui/dbgui.h"
#include "cmdbuf-sapp.glsl.h"

#define OFFSCREEN_COLOR_FORMAT (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_DEPTH_FORMAT (SG_PIXELFORMAT_DEPTH)
#define OFFSCREEN_SAMPLE_COUNT (1)
#define DISPLAY_SAMPLE_COUNT (4)

static struct {
    struct {
        scb_cmdbuf cmdbuf;
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        scb_cmdbuf cmdbuf;
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } display;
    sshape_element_range_t donut;
    sshape_element_range_t sphere;
    float rx, ry;
} state;

static void setup_render_resources(void);
static mat44_t compute_mvp(float rx, float ry, float aspect, float eye_dist);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    // setup sokol-cmdbuf, we only need 2 command buffers
    scb_setup(&(scb_desc){
        .cmdbuf_pool_size = 2,
        .logger.func = slog_func,
    });

    // create one command buffer for the offscreen render commands,
    // and one command buffer for the display pass render commands
    state.offscreen.cmdbuf = scb_make_cmdbuf(&(scb_cmdbuf_desc){
        .size = 512,
        .label = "offscreen-cmdbuf",
    });
    state.display.cmdbuf = scb_make_cmdbuf(&(scb_cmdbuf_desc){
        .size = 512,
        .label = "display-cmdbuf",
    });

    // setup render resources (moved out into separate function because
    // for this sample the creation code isn't important)
    setup_render_resources();
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t;
    state.ry += 2.0f * t;

    // record offscreen-rendering commands
    {
        const vs_params_t vs_params = (vs_params_t) {
            .mvp = compute_mvp(state.rx, state.ry, 1.0f, 2.5f)
        };
        scb_cmdbuf cb = state.offscreen.cmdbuf;
        scb_apply_pipeline(cb, state.offscreen.pip);
        scb_apply_bindings(cb, &state.offscreen.bind);
        scb_apply_uniforms(cb, UB_vs_params, &SG_RANGE(vs_params));
        scb_draw(cb, state.donut.base_element, state.donut.num_elements, 1);
    }
    // record display-rendering commands
    {
        const vs_params_t vs_params = {
            .mvp = compute_mvp(-state.rx * 0.25f, state.ry * 0.25f, sapp_widthf()/sapp_heightf(), 1.5f)
        };
        scb_cmdbuf cb = state.display.cmdbuf;
        scb_apply_pipeline(cb, state.display.pip);
        scb_apply_bindings(cb, &state.display.bind);
        scb_apply_uniforms(cb, UB_vs_params, &SG_RANGE(vs_params));
        scb_draw(cb, state.sphere.base_element, state.sphere.num_elements, 1);
    }

    // now submit the offscreen command buffer into a sokol-gfx offscreen render pass
    sg_begin_pass(&state.offscreen.pass);
    scb_submit(state.offscreen.cmdbuf);
    sg_end_pass();

    // ...and the display render pass
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    scb_submit(state.display.cmdbuf);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    scb_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

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

static void setup_render_resources(void) {
    // default pass action: clear to blue-ish
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.45f, 0.65f, 1.0f }
        }
    };

    // setup offscreen pass resources
    sg_image_desc img_desc = {
        .usage = { .color_attachment = true },
        .width = 256,
        .height = 256,
        .pixel_format = OFFSCREEN_COLOR_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "color-image"
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = OFFSCREEN_DEPTH_FORMAT;
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
    uint8_t vertices[SSHAPE_MAX_VERTEX_SIZE * 4000] = { 0 };
    uint16_t indices[24000] = { 0 };
    sshape_state_t shp = {
        .disable.colors = true,
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer  = SSHAPE_RANGE(indices),
    };
    sshape_build_torus(&shp, &(sshape_torus_t){
        .radius = 0.5f,
        .ring_radius = 0.3f,
        .sides = 20,
        .rings = 36,
    });
    state.donut = sshape_element_range(&shp);
    sshape_build_sphere(&shp, &(sshape_sphere_t) {
        .radius = 0.5f,
        .slices = 72,
        .stacks = 40
    });
    state.sphere = sshape_element_range(&shp);

    sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&shp);
    sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&shp);
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
            .buffers[0] = sshape_vertex_buffer_layout_state(&shp),
            .attrs = {
                [ATTR_offscreen_position] = sshape_position_vertex_attr_state(&shp),
                [ATTR_offscreen_normal] = sshape_normal_vertex_attr_state(&shp)
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
        .label = "offscreen-pipeline"
    });

    // and another pipeline-state-object for the default pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0] = sshape_vertex_buffer_layout_state(&shp),
            .attrs = {
                [ATTR_default_position] = sshape_position_vertex_attr_state(&shp),
                [ATTR_default_normal] = sshape_normal_vertex_attr_state(&shp),
                [ATTR_default_texcoord0] = sshape_texcoord_vertex_attr_state(&shp)
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
        .window_title = "cmdbuf-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
