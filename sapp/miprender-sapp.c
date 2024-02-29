//------------------------------------------------------------------------------
//  miprender-sapp.c
//
// Rendering into into texture mipmaps.
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
#include "miprender-sapp.glsl.h"

#define IMG_WIDTH (512)
#define IMG_HEIGHT (512)
#define IMG_NUM_MIPMAPS (9)

#define SHAPE_BOX (0)
#define SHAPE_DONUT (1)
#define SHAPE_SPHERE (2)
#define NUM_SHAPES (3)

static struct {
    float rx, ry;
    double time;
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_image img;
    sg_sampler smp;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
        sg_bindings bindings;
        sshape_element_range_t shapes[NUM_SHAPES];
        sg_attachments attachments[IMG_NUM_MIPMAPS];
    } offscreen;
    struct {
        sg_pipeline pip;
        sg_pass_action pass_action;
        sg_bindings bindings;
        sshape_element_range_t plane;
    } display;
} state;

static vs_params_t compute_offscreen_vsparams(void);
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
    buf = sshape_build_sphere(&buf, &(sshape_sphere_t){ .radius = 1.0f, .slices = 36, .stacks = 20 });
    state.offscreen.shapes[SHAPE_SPHERE] = sshape_element_range(&buf);
    buf = sshape_build_plane(&buf, &(sshape_plane_t){ .width = 2.0f, .depth = 2.0f });
    state.display.plane = sshape_element_range(&buf);
    assert(buf.valid);

    // create one vertex- and one index-buffer for all shapes
    const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
    const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
    state.vbuf = sg_make_buffer(&vbuf_desc);
    state.ibuf = sg_make_buffer(&ibuf_desc);

    // create an offscreen render target with a complete mipmap chain
    state.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .num_mipmaps = IMG_NUM_MIPMAPS,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
    });

    // we also need a matching depth buffer image
    sg_image depth_img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = IMG_WIDTH,
        .height = IMG_HEIGHT,
        .num_mipmaps = IMG_NUM_MIPMAPS,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
    });

    // create a sampler which smoothly blends between mipmaps
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    // create render pass objects for offscreen rendering (one pass per mipmap)
    for (int mip_level = 0; mip_level < IMG_NUM_MIPMAPS; mip_level++) {
        state.offscreen.attachments[mip_level] = sg_make_attachments(&(sg_attachments_desc){
            .colors[0] = {
                .image = state.img,
                .mip_level = mip_level
            },
            .depth_stencil = {
                .image = depth_img,
                .mip_level = mip_level
            },
        });
    }

    // a pipeline object for the offscreen passes
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(sshape_vertex_t),
            .attrs = {
                [ATTR_vs_offscreen_in_pos] = sshape_position_vertex_attr_state(),
                [ATTR_vs_offscreen_in_nrm] = sshape_normal_vertex_attr_state(),
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
    });

    // ...and a pipeline object for the display pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(sshape_vertex_t),
            .attrs = {
                [ATTR_vs_display_in_pos] = sshape_position_vertex_attr_state(),
                [ATTR_vs_display_in_uv] = sshape_texcoord_vertex_attr_state(),
            },
        },
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        }
    });

    // initialize resource bindings
    state.offscreen.bindings = (sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
    };
    state.display.bindings = (sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
        .fs = {
            .images[SLOT_tex] = state.img,
            .samplers[SLOT_smp] = state.smp,
        }
    };

    // initialize pass actions
    state.offscreen.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 0.5f, 1.0f } },
    };
    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };
}

static void frame(void) {
    double dt = sapp_frame_duration();
    state.time += dt;
    state.rx += (float)(dt * 20.0f);
    state.ry += (float)(dt * 40.0f);

    const vs_params_t offscreen_vsparams = compute_offscreen_vsparams();
    const vs_params_t display_vsparams = compute_display_vsparams();

    // render different shapes into each mipmap level
    for (int i = 0; i < IMG_NUM_MIPMAPS; i++) {
        sg_begin_pass(&(sg_pass) {
            .action = state.offscreen.pass_action,
            .attachments = state.offscreen.attachments[i],
        });
        sg_apply_pipeline(state.offscreen.pip);
        sg_apply_bindings(&state.offscreen.bindings);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(offscreen_vsparams));
        const sshape_element_range_t shape = state.offscreen.shapes[i % NUM_SHAPES];
        sg_draw(shape.base_element, shape.num_elements, 1);
        sg_end_pass();
    }

    // default pass: render a textured plane that moves back and forth to use different mipmap levels
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bindings);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(display_vsparams));
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
static vs_params_t compute_offscreen_vsparams(void) {
    hmm_mat4 proj = HMM_Perspective(60.0f, 1.0f, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 3.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 0.0f, 1.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    return (vs_params_t){ .mvp = HMM_MultiplyMat4(view_proj, model) };
}

// compute a model-view-projection matrix with display aspect ratio
static vs_params_t compute_display_vsparams(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const float scale = (HMM_SinF((float)state.time) + 1.0f) * 0.5f;
    hmm_mat4 proj = HMM_Perspective(40.0f, w/h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 3.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 model = HMM_Rotate(90.0f, HMM_Vec3(1.0f, 0.0f, 0.0f));
    model = HMM_MultiplyMat4(HMM_Scale(HMM_Vec3(scale, scale, 1.0f)), model);
    return (vs_params_t){ .mvp = HMM_MultiplyMat4(view_proj, model) };
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
        .sample_count = 1,
        .window_title = "miprender-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}