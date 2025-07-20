//------------------------------------------------------------------------------
//  mrt-pixelformats-sapp.c
//
//  Test/demonstrate multiple-render-target rendering with different
//  pixel formats.
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
#include "mrt-pixelformats-sapp.glsl.h"
#include <assert.h>

// render target pixel formats
#define DEPTH_PIXEL_FORMAT SG_PIXELFORMAT_R32F
#define NORMAL_PIXEL_FORMAT SG_PIXELFORMAT_RGBA16F
#define COLOR_PIXEL_FORMAT SG_PIXELFORMAT_RGBA8

// size of offscreen render targets
#define OFFSCREEN_WIDTH (512)
#define OFFSCREEN_HEIGHT (512)

// a helper struct which bundles an image, a color attachment view and a texture view
typedef struct {
    sg_image img;
    sg_view att_view;
    sg_view tex_view;
} image_and_views_t;

static struct {
    bool features_ok;
    struct {
        image_and_views_t depth;
        image_and_views_t normal;
        image_and_views_t color;
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
        hmm_mat4 view_proj;
        sshape_element_range_t donut;
    } offscreen;
    struct {
        sg_pass_action pass_action;
        sg_buffer vbuf;
        sg_sampler smp;
        sg_pipeline pip;
    } display;
    float rx, ry;
} state;

static image_and_views_t make_image_and_views(const sg_image_desc* img_desc) {
    sg_image img = sg_make_image(img_desc);
    sg_view att_view = sg_make_view(&(sg_view_desc){
        .color_attachment = { .image = img },
    });
    sg_view tex_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = img },
    });
    return (image_and_views_t){ .img = img, .att_view = att_view, .tex_view = tex_view };
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // check if requires features are supported
    state.features_ok = sg_query_pixelformat(DEPTH_PIXEL_FORMAT).render &&
                        sg_query_pixelformat(NORMAL_PIXEL_FORMAT).render &&
                        sg_query_pixelformat(COLOR_PIXEL_FORMAT).render;
    if (!state.features_ok) {
        return;
    }

    // setup resources for offscreen rendering
    {
        // create 3 render target image and texture views with different formats
        sg_image_desc img_desc = {
            .usage.color_attachment = true,
            .pixel_format = DEPTH_PIXEL_FORMAT,
            .width = OFFSCREEN_WIDTH,
            .height = OFFSCREEN_HEIGHT,
            .sample_count = 1,
        };
        state.offscreen.depth = make_image_and_views(&img_desc);
        img_desc.pixel_format = NORMAL_PIXEL_FORMAT;
        state.offscreen.normal = make_image_and_views(&img_desc);
        img_desc.pixel_format = COLOR_PIXEL_FORMAT;
        state.offscreen.color = make_image_and_views(&img_desc);
        img_desc.usage = (sg_image_usage){ .depth_stencil_attachment = true };
        img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
        sg_image zbuf_img = sg_make_image(&img_desc);
        sg_view zbuf_view = sg_make_view(&(sg_view_desc){
            .depth_stencil_attachment = { .image = zbuf_img },
        });

        // a render pass descriptor for mrt rendering
        state.offscreen.pass = (sg_pass){
            .action.colors = {
                [0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 0.0f } },
                [1] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 0.0f } },
                [2] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 0.0f } },
            },
            .attachments = {
                .colors[0] = state.offscreen.depth.att_view,
                .colors[1] = state.offscreen.normal.att_view,
                .colors[2] = state.offscreen.color.att_view,
                .depth_stencil = zbuf_view,
            },
        };

        // create a shape to render into the offscreen render target
        sshape_vertex_t vertices[3000] = {0};
        uint16_t indices[6000] = {0};
        sshape_buffer_t buf = {
            .vertices.buffer = SSHAPE_RANGE(vertices),
            .indices.buffer = SSHAPE_RANGE(indices)
        };
        buf = sshape_build_torus(&buf, &(sshape_torus_t){
            .radius = 0.5f,
            .ring_radius = 0.3f,
            .sides = 20,
            .rings = 36,
            .random_colors = true,
        });
        assert(buf.valid);
        state.offscreen.donut = sshape_element_range(&buf);
        const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
        const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
        sg_buffer vbuf = sg_make_buffer(&vbuf_desc);
        sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

        // create shader and pipeline object for offscreen MRT rendering
        state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
            .index_type = SG_INDEXTYPE_UINT16,
            .cull_mode = SG_CULLMODE_BACK,
            .layout = {
                .buffers[0] = sshape_vertex_buffer_layout_state(),
                .attrs = {
                    [ATTR_offscreen_in_pos]    = sshape_position_vertex_attr_state(),
                    [ATTR_offscreen_in_normal] = sshape_normal_vertex_attr_state(),
                    [ATTR_offscreen_in_color]  = sshape_color_vertex_attr_state()
                }
            },
            .depth = {
                .pixel_format = SG_PIXELFORMAT_DEPTH,
                .write_enabled = true,
                .compare = SG_COMPAREFUNC_LESS_EQUAL
            },
            .color_count = 3,
            .colors = {
                [0].pixel_format = DEPTH_PIXEL_FORMAT,
                [1].pixel_format = NORMAL_PIXEL_FORMAT,
                [2].pixel_format = COLOR_PIXEL_FORMAT,
            }
        });

        // resource bindings for offscreen rendering
        state.offscreen.bind = (sg_bindings) {
            .vertex_buffers[0] = vbuf,
            .index_buffer = ibuf,
        };

        // constant view_proj matrix for offscreen rendering
        hmm_mat4 proj = HMM_Perspective(60.0f, 1.0f, 0.01f, 5.0f);
        hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 2.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
        state.offscreen.view_proj = HMM_MultiplyMat4(proj, view);
    }

    // setup resources for rendering to the display
    {
        state.display.pass_action = (sg_pass_action) {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.25f, 0.5f, 0.75f, 1.0f } }
        };

        // a vertex buffer for rendering a quad
        float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
        state.display.vbuf = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(quad_vertices),
        });

        // a shader and pipeline object to render a quad
        state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = sg_make_shader(quad_shader_desc(sg_query_backend())),
            .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
            .layout.attrs[ATTR_quad_pos].format = SG_VERTEXFORMAT_FLOAT2,
        });

        // a sampler for sampling the offscreen render target as textures
        state.display.smp = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        });
    }

}

static void draw_fallback() {
    const sg_pass_action pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f }}
    };
    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = sglue_swapchain() });
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static offscreen_params_t compute_offscreen_params(void) {
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rzm = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 0.0f, 1.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rzm);
    return (offscreen_params_t) {
        .mvp = HMM_MultiplyMat4(state.offscreen.view_proj, model)
    };
}

static void frame(void) {
    if (!state.features_ok) {
        draw_fallback();
        return;
    }
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t;
    state.ry += 2.0f * t;

    // render donut shape into MRT offscreen render targets
    const offscreen_params_t offscreen_params = compute_offscreen_params();
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(UB_offscreen_params, &SG_RANGE(offscreen_params));
    sg_draw(state.offscreen.donut.base_element, state.offscreen.donut.num_elements, 1);
    sg_end_pass();

    // render offscreen render targets to display
    const int disp_width = sapp_width();
    const int disp_height = sapp_height();
    const int quad_width = disp_width / 4;
    const int quad_height = quad_width;
    const int quad_gap = (disp_width - quad_width*3) / 4;
    const int x0 = quad_gap;
    const int y0 = (disp_height - quad_height) / 2;
    sg_bindings bindings = {
        .vertex_buffers[0] = state.display.vbuf,
        .samplers[SMP_smp] = state.display.smp,
    };

    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    quad_params_t quad_params = { .color_bias = 0.0f, .color_scale = 1.0f };
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(x0 + i*(quad_width+quad_gap), y0, quad_width, quad_height, true);
        switch (i) {
            case 0:
                bindings.views[VIEW_tex] = state.offscreen.depth.tex_view;
                quad_params.color_bias = 0.0f;
                quad_params.color_scale = 0.5f;
                break;
            case 1:
                bindings.views[VIEW_tex] = state.offscreen.normal.tex_view;
                quad_params.color_bias = 1.0f;
                quad_params.color_scale = 0.5f;
                break;
            case 2:
                bindings.views[VIEW_tex] = state.offscreen.color.tex_view;
                quad_params.color_bias = 0.0f;
                quad_params.color_scale = 1.0f;
                break;
        }
        sg_apply_uniforms(UB_quad_params, &SG_RANGE(quad_params));
        sg_apply_bindings(&bindings);
        sg_draw(0, 4, 1);
    }
    sg_apply_viewport(0, 0, disp_width, disp_height, true);
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
        .window_title = "MRT Pixelformats",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
