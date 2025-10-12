//------------------------------------------------------------------------------
//  layerrender-sapp.c
//
//  Rendering into texture-array layers.
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
#include "layerrender-sapp.glsl.h"
#include <stdio.h> // snprintf

#define IMG_WIDTH (512)
#define IMG_HEIGHT (512)
#define IMG_NUM_LAYERS (3)

#define SHAPE_BOX (0)
#define SHAPE_DONUT (1)
#define SHAPE_CYLINDER (2)
#define NUM_SHAPES (3)

static struct {
    float rx, ry;
    double time;
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_view tex_view;
    sg_sampler smp;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
        sg_bindings bindings;
        sg_view color_att_views[IMG_NUM_LAYERS];
        sg_view depth_att_view;
        sshape_element_range_t shapes[NUM_SHAPES];
    } offscreen;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
        sg_bindings bindings;
        sshape_element_range_t plane;
    } display;
} state;

static vs_params_t compute_offscreen_vsparams(float rx, float ry);
static vs_params_t compute_display_vsparams(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // setup a couple of shape geometries
    static sshape_vertex_t vertices[4 * 1024];
    static uint16_t indices[12 * 1024];
    sshape_buffer_t buf = {
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer = SSHAPE_RANGE(indices),
    };
    buf = sshape_build_box(&buf, &(sshape_box_t){ .width = 1.5f, .height = 1.5f, .depth = 1.5f });
    state.offscreen.shapes[SHAPE_BOX] = sshape_element_range(&buf);
    buf = sshape_build_torus(&buf, &(sshape_torus_t){ .radius = 1.0f, .ring_radius = 0.3f, .rings = 36, .sides = 18 });
    state.offscreen.shapes[SHAPE_DONUT] = sshape_element_range(&buf);
    buf = sshape_build_cylinder(&buf, &(sshape_cylinder_t){ .radius = 1.0f, .height = 1.5f, .slices = 36, .stacks = 1 });
    state.offscreen.shapes[SHAPE_CYLINDER] = sshape_element_range(&buf);
    buf = sshape_build_plane(&buf, &(sshape_plane_t){ .width = 2.0f, .depth = 2.0f });
    state.display.plane = sshape_element_range(&buf);
    assert(buf.valid);

    // create one vertex- and one index-buffer for all shapes
    sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
    vbuf_desc.label = "shape-vertices";
    sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
    ibuf_desc.label = "shape-indices";
    state.vbuf = sg_make_buffer(&vbuf_desc);
    state.ibuf = sg_make_buffer(&ibuf_desc);

    // create an array-texture as render target
    sg_image color_img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .type = SG_IMAGETYPE_ARRAY,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .num_slices = IMG_NUM_LAYERS,
        .num_mipmaps = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "color-image",
    });

    // ...and a matching depth buffer image
    sg_image depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .num_mipmaps = 1,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
        .label = "depth-image",
    });

    // a sampler for sampling the array texture
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "sampler",
    });

    // view objects (one color attachment per texture layer, one depth-stencil attachment, one texture view)
    for (int i = 0; i < IMG_NUM_LAYERS; i++) {
        char label[32];
        snprintf(label, sizeof(label), "color-attachment-slice-%d", i);
        state.offscreen.color_att_views[i] = sg_make_view(&(sg_view_desc){
            .color_attachment = { .image = color_img, .slice = i },
            .label = label,
        });
    }
    state.offscreen.depth_att_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment.image = depth_img,
        .label = "depth-attachemnt",
    });
    state.tex_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = color_img },
        .label = "texture-view",
    });

    // a pipeline object for the offscreen pass
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(sshape_vertex_t),
            .attrs = {
                [ATTR_offscreen_in_pos] = sshape_position_vertex_attr_state(),
                [ATTR_offscreen_in_nrm] = sshape_normal_vertex_attr_state(),
            },
        },
        .shader = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = 1,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .pixel_format = SG_PIXELFORMAT_DEPTH,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "offscreen-pipeline",
    });

    // ...and a pipeline object for the display pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(sshape_vertex_t),
            .attrs = {
                [ATTR_display_in_pos] = sshape_position_vertex_attr_state(),
                [ATTR_display_in_uv] = sshape_texcoord_vertex_attr_state(),
            },
        },
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = 1,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .label = "display-pipeline",
    });

    // initialize resource bindings
    state.offscreen.bindings = (sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
    };
    state.display.bindings = (sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
        .views[VIEW_tex] = state.tex_view,
        .samplers[SMP_smp] = state.smp,
    };

    // initialize pass actions
    state.offscreen.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 0.5f, 1.0f } },
    };
    state.display.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };
}

static void frame(void) {
    double dt = sapp_frame_duration();
    state.time += dt;
    state.rx += (float)(dt * 20.0f);
    state.ry += (float)(dt * 40.0f);

    const vs_params_t display_vsparams = compute_display_vsparams();

    // render different shapes into each texture array layer
    for (int i = 0; i < IMG_NUM_LAYERS; i++) {
        sg_begin_pass(&(sg_pass){
            .action = state.offscreen.pass_action,
            .attachments = {
                .colors[0] = state.offscreen.color_att_views[i],
                .depth_stencil = state.offscreen.depth_att_view,
            },
        });
        sg_apply_pipeline(state.offscreen.pip);
        sg_apply_bindings(&state.offscreen.bindings);
        float rx = state.rx;
        float ry = state.ry;
        switch (i) {
            case 0: break;
            case 1: rx = -rx; break;
            default: ry = -ry; break;
        }
        const vs_params_t offscreen_vsparams = compute_offscreen_vsparams(rx, ry);
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(offscreen_vsparams));
        const sshape_element_range_t shape = state.offscreen.shapes[i];
        sg_draw(shape.base_element, shape.num_elements, 1);
        sg_end_pass();
    }

    // default pass: render a textured plane which accesses all texture layers in the fragment shader
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain_next() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bindings);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(display_vsparams));
    sg_draw(state.display.plane.base_element, state.display.plane.num_elements, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

// compute a model-view-projection matrix for offscreen rendering (aspect ratio 1:1)
static vs_params_t compute_offscreen_vsparams(float rx, float ry) {
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), 1.0f, 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, 3.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    const mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    const mat44_t rym = mat44_rotation_z(vm_radians(ry));
    const mat44_t model = vm_mul(rym, rxm);
    return (vs_params_t){ .mvp = vm_mul(model, view_proj) };
}

// compute a model-view-projection matrix with display aspect ratio
static vs_params_t compute_display_vsparams(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(40.0f), w/h, 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    const mat44_t model = mat44_rotation_x(vm_radians(90.0f));
    return (vs_params_t){ .mvp = vm_mul(model, view_proj) };
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
        .sample_count = 1,
        .window_title = "layerrender-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
