//------------------------------------------------------------------------------
//  mrt-sapp.c
//  Rendering with multi-rendertargets, and recreating render targets
//  when window size changes.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath.h"
#include "dbgui/dbgui.h"
#include "mrt-sapp.glsl.h"
#include <stddef.h> /* offsetof */
#include <assert.h>

#define OFFSCREEN_SAMPLE_COUNT (4)
#define NUM_MRTS (3)

static struct {
    struct {
        sg_pipeline pip;
        sg_bindings bind;
        sg_pass pass;
    } offscreen;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
        sg_pass_action pass_action;
    } display;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
    } dbg;
    struct {
        sg_image color[NUM_MRTS];
        sg_image resolve[NUM_MRTS];
        sg_image depth;
    } images;
    float rx, ry;
} state;

typedef struct {
    float x, y, z, b;
} vertex_t;

static void reinit_attachments(int width, int height);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // a pass action for the default render pass
    state.display.pass_action = (sg_pass_action) {
        .colors[0].load_action = SG_LOADACTION_DONTCARE,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    // pre-allocate all image and view handles upfront, the actual
    // initialization will then happen in `reinit_attachments()` which both
    // called from init() and when the window size changes
    //
    // NOTE: we could just as well call destroy/make on window resize,
    // and in reality this wouldn't make much of a difference, in this sample
    // the pre-allocate + uninit/init is used for better test coverage
    for (int i = 0; i < NUM_MRTS; i++) {
        state.images.color[i] = sg_alloc_image();
        state.images.resolve[i] = sg_alloc_image();
        state.offscreen.pass.attachments.colors[i] = sg_alloc_view();
        state.offscreen.pass.attachments.resolves[i] = sg_alloc_view();
        state.display.bind.views[VIEW_tex0 + i] = sg_alloc_view();
    }
    state.images.depth = sg_alloc_image();
    state.offscreen.pass.attachments.depth_stencil = sg_alloc_view();

    // initialize pass attachment images and views
    reinit_attachments(sapp_width(), sapp_height());

    // create a vertex buffer for the cube
    vertex_t cube_vertices[] = {
        // pos + brightness
        { -1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f,  1.0f, -1.0f,   1.0f },
        { -1.0f,  1.0f, -1.0f,   1.0f },

        { -1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f,  1.0f,  1.0f,   0.8f },
        { -1.0f,  1.0f,  1.0f,   0.8f },

        { -1.0f, -1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f,  1.0f,   0.6f },
        { -1.0f, -1.0f,  1.0f,   0.6f },

        { 1.0f, -1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f,  1.0f,    0.4f },
        { 1.0f, -1.0f,  1.0f,    0.4f },

        { -1.0f, -1.0f, -1.0f,   0.5f },
        { -1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f, -1.0f,   0.5f },

        { -1.0f,  1.0f, -1.0f,   0.7f },
        { -1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f, -1.0f,   0.7f },
    };
    state.offscreen.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices),
        .label = "cube vertices"
    });

    // ...and an index buffer for the cube
    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.offscreen.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices),
        .label = "cube indices"
    });

    // pipeline and shader object for the offscreen-rendered cube
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [ATTR_offscreen_pos]     = { .offset=offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [ATTR_offscreen_bright0] = { .offset=offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .color_count = NUM_MRTS,
        .label = "offscreen pipeline"
    });

    // a pass action for the offscreen pass (since the MSAA render targets will be resolved
    // into a texture their content doesn't need to be stored)
    state.offscreen.pass.action = (sg_pass_action) {
        .colors = {
            [0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .store_action = SG_STOREACTION_DONTCARE,
                .clear_value = { 0.25f, 0.0f, 0.0f, 1.0f }
            },
            [1] = {
                .load_action = SG_LOADACTION_CLEAR,
                .store_action = SG_STOREACTION_DONTCARE,
                .clear_value = { 0.0f, 0.25f, 0.0f, 1.0f }
            },
            [2] = {
                .load_action = SG_LOADACTION_CLEAR,
                .store_action = SG_STOREACTION_DONTCARE,
                .clear_value = { 0.0f, 0.0f, 0.25f, 1.0f }
            }
        }
    };

    // a vertex buffer to render a fullscreen rectangle
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    const sg_buffer quad_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices),
        .label = "quad vertices"
    });

    // a pipeline and shader object to render the fullscreen quad
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(fsq_shader_desc(sg_query_backend())),
        .layout = {
            .attrs[ATTR_fsq_pos].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "fullscreen quad pipeline"
    });

    // a sampler object to sample the offscreen render targets as textures
    const sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "sampler",
    });

    // complete the resource bindings for the fullscreen quad
    state.display.bind.vertex_buffers[0] = quad_vbuf;
    state.display.bind.samplers[SMP_smp] = smp;

    // pipeline and resource bindings to render debug-visualization quads
    state.dbg.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(dbg_shader_desc(sg_query_backend())),
        .layout = {
            .attrs[ATTR_dbg_pos].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "dbgvis quad pipeline"
    }),
    state.dbg.bind = (sg_bindings){
        .vertex_buffers[0] = quad_vbuf,
        .samplers[SMP_smp] = smp,
        // texture views will be filled right before rendering
    };
}

static void frame(void) {
    // view-projection matrix
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), sapp_widthf()/sapp_heightf(), 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);

    // shader parameters
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const mat44_t rxm = mat44_rotation_x(vm_radians(state.rx));
    const mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    const mat44_t model = vm_mul(rym, rxm);
    const offscreen_params_t offscreen_params = { .mvp = vm_mul(model, view_proj) };
    const fsq_params_t fsq_params = {
        .offset = vec2(vm_sin(state.rx*0.01f)*0.1f, vm_sin(state.ry*0.01f)*0.1f)
    };

    // render cube into MRT offscreen render targets
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(UB_offscreen_params, &SG_RANGE(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // render fullscreen quad with the 'composed image', plus 3 small debug-view quads
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(UB_fsq_params, &SG_RANGE(fsq_params));
    sg_draw(0, 4, 1);
    sg_apply_pipeline(state.dbg.pip);
    for (int i = 0; i < NUM_MRTS; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        state.dbg.bind.views[VIEW_tex] = state.display.bind.views[VIEW_tex0 + i];
        sg_apply_bindings(&state.dbg.bind);
        sg_draw(0, 4, 1);
    }
    sg_apply_viewport(0, 0, sapp_width(), sapp_height(), false);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

// listen for window-resize events and recreate offscreen rendertargets
static void event(const sapp_event* e) {
    if (e->type == SAPP_EVENTTYPE_RESIZED) {
        reinit_attachments(e->framebuffer_width, e->framebuffer_height);
    }
    __dbgui_event(e);
}

// called initially and when window size changes, will re-initialize
// the offscreen render target images to a new size and then re-initialize
// the associated view objects
static void reinit_attachments(int width, int height) {
    // uninitialize the render target images and associated views (NOTE: it's fine to call
    // uninit on resources in ALLOC state)
    for (int i = 0; i < NUM_MRTS; i++) {
        sg_uninit_image(state.images.color[i]);
        sg_uninit_image(state.images.resolve[i]);
        sg_uninit_view(state.offscreen.pass.attachments.colors[i]);
        sg_uninit_view(state.offscreen.pass.attachments.resolves[i]);
        sg_uninit_view(state.display.bind.views[VIEW_tex + i]);
    }
    sg_uninit_image(state.images.depth);
    sg_uninit_view(state.offscreen.pass.attachments.depth_stencil);

    // ...next initialize images with the new size and re-init their associated handles
    const char* msaa_image_labels[NUM_MRTS] = { "msaa-image-red", "msaa-image-green", "msaa-image-blue" };
    const char* resolve_image_labels[NUM_MRTS] = { "resolve-image-red", "resolve-image-green", "resolve-image-blue" };
    const char* color_attachment_labels[NUM_MRTS] = { "color-attachment-red", "color-attachment-green", "color-attachment-blue" };
    const char* resolve_attachment_labels[NUM_MRTS] = { "resolve-attachment-red", "resolve-attachment-green", "resolve-attachment-blue" };
    const char* tex_view_labels[NUM_MRTS] = { "texture-view-red", "texture-view-green", "texture-view-blue" };
    for (int i = 0; i < NUM_MRTS; i++) {
        sg_init_image(state.images.color[i], &(sg_image_desc){
            .usage.color_attachment = true,
            .width = width,
            .height = height,
            .sample_count = OFFSCREEN_SAMPLE_COUNT,
            .label = msaa_image_labels[i],
        });
        sg_init_image(state.images.resolve[i], &(sg_image_desc){
            .usage.resolve_attachment = true,
            .width = width,
            .height = height,
            .sample_count = 1,
            .label = resolve_image_labels[i],
        });
        sg_init_view(state.offscreen.pass.attachments.colors[i], &(sg_view_desc){
            .color_attachment = { .image = state.images.color[i] },
            .label = color_attachment_labels[i],
        });
        sg_init_view(state.offscreen.pass.attachments.resolves[i], &(sg_view_desc){
            .resolve_attachment = { .image = state.images.resolve[i] },
            .label = resolve_attachment_labels[i],
        });
        sg_init_view(state.display.bind.views[VIEW_tex0 + i], &(sg_view_desc){
            .texture = { .image = state.images.resolve[i] },
            .label = tex_view_labels[i],
        });
    }
    sg_init_image(state.images.depth, &(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = width,
        .height = height,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "depth-image",
    });
    sg_init_view(state.offscreen.pass.attachments.depth_stencil, &(sg_view_desc){
        .depth_stencil_attachment = { .image = state.images.depth },
        .label = "depth-attachment",
    });
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "MRT Rendering (sokol-app)",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
