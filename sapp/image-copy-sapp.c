//------------------------------------------------------------------------------
//  image-copy-sapp.c
//
//  Demonstrates sg_copy_image_to_image(). Renders a rotating cube into
//  a small offscreen render target and takes snapshots via
//  sg_copy_image_to_image into a bigger render target which is then
//  rendered as quad to the display.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_LETTERBOX_IMPL
#include "sokol_letterbox.h"
#include "dbgui/dbgui.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "image-copy-sapp.glsl.h"

#define SMALL_RENDER_SIZE (64)
#define BIG_RENDER_SIZE (SMALL_RENDER_SIZE * 8)

static struct {
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
    } cube;
    struct {
        sg_image color_img;
        sg_image depth_img;
        sg_pass pass;
    } offscreen;
    struct {
        sg_image img;
        sg_view tex_view;
        sg_sampler smp;
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
    float rx, ry;
} state;

// cube vertices and indices
static const float cube_vertices[] = {
    -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
     1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
     1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
    -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

    -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
     1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
     1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
    -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

    -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

    1.0, -1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
    1.0,  1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
    1.0,  1.0,  1.0,    1.0, 0.5, 0.0, 1.0,
    1.0, -1.0,  1.0,    1.0, 0.5, 0.0, 1.0,

    -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
    -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
     1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
     1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

    -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
    -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
     1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
     1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
};

static const uint16_t cube_indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

static cube_vs_params_t compute_vsparams(float rx, float ry);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    _dbgui_setup();

    // create a small render target and depth-buffer image as copy-src
    state.offscreen.color_img = sg_make_image(&(sg_image_desc){
        .usage = {
            .color_attachment = true,
            .copy_src = true,
        },
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .width = SMALL_RENDER_SIZE,
        .height = SMALL_RENDER_SIZE,
        .sample_count = 1,
        .label = "copy-src-image",
    });
    state.offscreen.depth_img = sg_make_image(&(sg_image_desc){
        .usage = {
            .depth_stencil_attachment = true,
        },
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .width = SMALL_RENDER_SIZE,
        .height = SMALL_RENDER_SIZE,
        .label = "depth-buffer",
    });

    // and the pass struct (pass-attachments and pass-action)
    state.offscreen.pass = (sg_pass){
        .attachments = {
            .colors[0] = sg_make_view(&(sg_view_desc){
                .color_attachment.image = state.offscreen.color_img,
                .label = "color-attachment-view",
            }),
            .depth_stencil = sg_make_view(&(sg_view_desc){
                .depth_stencil_attachment.image = state.offscreen.depth_img,
                .label = "depth-attachment-view",
            }),
        },
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
            },
        }
    };

    // ...and a large image as copy-dst and its texture view and sampler
    state.display.img = sg_make_image(&(sg_image_desc){
        .usage = {
            .copy_dst = true,
        },
        .width = BIG_RENDER_SIZE,
        .height = BIG_RENDER_SIZE,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "copy-dst-image",
    });
    state.display.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = state.display.img,
        .label = "copy-dst-tex-view",
    });
    state.display.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "copy-dst-sampler",
    });

    // the display pass-action (clear to black)
    state.display.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    // create cube resources
    state.cube.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices),
        .label = "cube-vertices",
    });
    state.cube.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices),
        .label = "cube-indices",
    });
    state.cube.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(cube_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_cube_color0].format = SG_VERTEXFORMAT_FLOAT4,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "cube-pipeline",
    });

    // pipeline for rendering a 'fullscreen triangle'
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "display-pip",
    });
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const cube_vs_params_t cube_vs_params = compute_vsparams(state.rx, state.ry);

    // offscreen pass: render rotating cube into small render target
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.cube.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.cube.vbuf,
        .index_buffer = state.cube.ibuf,
    });
    sg_apply_uniforms(UB_cube_vs_params, &SG_RANGE(cube_vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    _dbgui_update();
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain()
    });
    // FIXME: viewport
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_tex] = state.display.tex_view,
        .samplers[SMP_smp] = state.display.smp,
    });
    sg_draw(0, 3, 1);
    _dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    _dbgui_shutdown();
    sg_shutdown();
}

static cube_vs_params_t compute_vsparams(float rx, float ry) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return (cube_vs_params_t){ .mvp = vm_mul(model, view_proj) };
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = _dbgui_event,
        .width = 512,
        .height = 512,
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .window_title = "image-copy-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
