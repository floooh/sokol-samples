//------------------------------------------------------------------------------
//  histogram-sapp.c
//
//  Demonstrates writing to storage buffer from within a fragment shader.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "util/fileutil.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "histogram-sapp.glsl.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define NUM_HISTOGRAM_BINS (256)
#define WORKGROUP_WIDTH (64)

static struct {
    struct {
        sg_buffer sbuf;
        sg_view sbuf_view;
        sg_pipeline clear_pip;
    } histogram;
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
        sg_pass pass;
        sg_image color_img;
        sg_image depth_img;
        sg_image png_img;
        sg_view png_tex_view;
        sg_sampler png_smp;
    } offscreen;
    struct {
        sg_pipeline canvas_pip;
        sg_pipeline hist_pip;
        sg_view color_tex_view;
        sg_sampler smp;
        sg_pass_action pass_action;
    } display;
    float rx, ry;
} state;

static uint8_t file_buffer[256 * 1024];

static void reinit_attachments(int width, int height);
static vs_params_t compute_vsparams(float rx, float ry);
static void fetch_callback(const sfetch_response_t*);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    _dbgui_setup();
    sfetch_setup(&(sfetch_desc_t){
        .logger.func = slog_func,
    });

    // a storage buffer and view for the histogram
    state.histogram.sbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .size = NUM_HISTOGRAM_BINS * sizeof(hist_bin_t),
        .label = "histogram-buffer",
    });
    state.histogram.sbuf_view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.histogram.sbuf,
        .label = "histogram-buffer-view",
    });

    // a shader and compute pipeline to clear the histogram buffer at the start of each frame
    state.histogram.clear_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(clear_shader_desc(sg_query_backend())),
        .label = "histogram-clear-pipeline",
    });

    // a cube vertex- and index-buffer
    float vertices[] = {
        -1.0, -1.0, -1.0,  0.0, 0.0,
         1.0, -1.0, -1.0,  1.0, 0.0,
         1.0,  1.0, -1.0,  1.0, 1.0,
        -1.0,  1.0, -1.0,  0.0, 1.0,

        -1.0, -1.0,  1.0,  0.0, 0.0,
         1.0, -1.0,  1.0,  1.0, 0.0,
         1.0,  1.0,  1.0,  1.0, 1.0,
        -1.0,  1.0,  1.0,  0.0, 1.0,

        -1.0, -1.0, -1.0,  0.0, 0.0,
        -1.0,  1.0, -1.0,  1.0, 0.0,
        -1.0,  1.0,  1.0,  1.0, 1.0,
        -1.0, -1.0,  1.0,  0.0, 1.0,

         1.0, -1.0, -1.0,  0.0, 0.0,
         1.0,  1.0, -1.0,  1.0, 0.0,
         1.0,  1.0,  1.0,  1.0, 1.0,
         1.0, -1.0,  1.0,  0.0, 1.0,

        -1.0, -1.0, -1.0,  0.0, 0.0,
        -1.0, -1.0,  1.0,  1.0, 0.0,
         1.0, -1.0,  1.0,  1.0, 1.0,
         1.0, -1.0, -1.0,  0.0, 1.0,

        -1.0,  1.0, -1.0,  0.0, 0.0,
        -1.0,  1.0,  1.0,  1.0, 0.0,
         1.0,  1.0,  1.0,  1.0, 1.0,
         1.0,  1.0, -1.0,  0.0, 1.0,
    };
    state.offscreen.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });

    // create an index buffer for the cube
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.offscreen.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // shader and pipeline to render the cube and update histogram bins
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(shape_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_shape_in_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_shape_in_uv].format = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .pixel_format = SG_PIXELFORMAT_DEPTH,
        },
        .label = "cube-pipeline",
    });

    // offscreen pass-action (important: clear to black so that
    // background doesn't contribute to histogram)
    state.offscreen.pass.action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1 } },
    };

    // image and sampler for the loaded texture
    state.offscreen.png_img = sg_alloc_image();
    state.offscreen.png_tex_view = sg_alloc_view();
    state.offscreen.png_smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "png-sampler",
    });

    // create pass attachment images and viws
    state.offscreen.color_img = sg_alloc_image();
    state.offscreen.depth_img = sg_alloc_image();
    state.offscreen.pass.attachments.colors[0] = sg_alloc_view();
    state.offscreen.pass.attachments.depth_stencil = sg_alloc_view();
    state.display.color_tex_view = sg_alloc_view();
    reinit_attachments(sapp_width(), sapp_height());

    // create a bufferless 'fullscreen-triangle' pipeline to render the offscreen image to the display
    state.display.canvas_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_canvas_shader_desc(sg_query_backend())),
        .label = "display-canvas-pipeline",
    });

    // a bufferless pipeline for rendering a single histogram bar
    state.display.hist_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_hist_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "display-histogram-pipeline",
    });

    // ...and a sample for rendering the fullscreen-triangle
    state.display.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "display-canvas-sampler",
    });

    // start loading texture file
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("baboon.png", path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(file_buffer)
    });
}

static void frame(void) {
    sfetch_dowork();

    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const vs_params_t vs_params = compute_vsparams(state.rx, state.ry);

    // a compute pass which clears the histogram storage buffer
    sg_begin_pass(&(sg_pass){ .compute = true });
    sg_apply_pipeline(state.histogram.clear_pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_hist_clear] = state.histogram.sbuf_view,
    });
    sg_dispatch(NUM_HISTOGRAM_BINS / WORKGROUP_WIDTH, 1, 1);
    sg_end_pass();

    // an offscreen pass which renders the cube and updates the histogram buffer
    // (NOTE: the texture may not be loaded yet, in that case the draw call
    // is automatically skipped)
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.offscreen.vbuf,
        .index_buffer = state.offscreen.ibuf,
        .views = {
            [VIEW_hist_out] = state.histogram.sbuf_view,
            [VIEW_shape_tex] = state.offscreen.png_tex_view,
        },
        .samplers[SMP_shape_smp] = state.offscreen.png_smp,
    });
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // the final display render pass
    _dbgui_update();
    sg_begin_pass(&(sg_pass){ .swapchain = sglue_swapchain() });
    // first 'blit' the offscreen render target as fullscreen triangle
    sg_apply_pipeline(state.display.canvas_pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_canvas_tex] = state.display.color_tex_view,
        .samplers[SMP_canvas_smp] = state.display.smp,
    });
    sg_draw(0, 3, 1);

    // the three histogram bars as instanced quads
    const int w = sapp_width() - 2 * 20;
    const int x = 20;
    const int h = sapp_height() / 10;
    const int y0 = sapp_height() - 4 * h;
    for (int i = 0; i < 3; i++) {
        const vs_hist_params_t vs_hist_params = {
            .channel = i,
        };
        sg_apply_viewport(x, y0 + i * h, w, h, true);
        sg_apply_pipeline(state.display.hist_pip);
        sg_apply_bindings(&(sg_bindings){
            .views[VIEW_hist_in] = state.histogram.sbuf_view,
        });
        sg_apply_uniforms(UB_vs_hist_params, &SG_RANGE(vs_hist_params));
        sg_draw(0, 4, NUM_HISTOGRAM_BINS-1);
    }

    _dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    _dbgui_shutdown();
    sg_shutdown();
}

static void event(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        reinit_attachments(ev->framebuffer_width, ev->framebuffer_height);
    }
    _dbgui_event(ev);
}

static void reinit_attachments(int width, int height) {
    sg_uninit_image(state.offscreen.color_img);
    sg_init_image(state.offscreen.color_img, &(sg_image_desc){
        .usage.color_attachment = true,
        .width = width,
        .height = height,
        .label = "color-image",
    });
    sg_uninit_image(state.offscreen.depth_img);
    sg_init_image(state.offscreen.depth_img, &(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = width,
        .height = height,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .label = "depth-image",
    });
    sg_uninit_view(state.display.color_tex_view);
    sg_init_view(state.display.color_tex_view, &(sg_view_desc){
        .texture.image = state.offscreen.color_img,
        .label = "color-image-texture-view",
    });
    sg_uninit_view(state.offscreen.pass.attachments.colors[0]);
    sg_init_view(state.offscreen.pass.attachments.colors[0], &(sg_view_desc){
        .color_attachment.image = state.offscreen.color_img,
        .label = "color-image-attachment-view",
    });
    sg_uninit_view(state.offscreen.pass.attachments.depth_stencil);
    sg_init_view(state.offscreen.pass.attachments.depth_stencil, &(sg_view_desc){
        .depth_stencil_attachment.image = state.offscreen.depth_img,
        .label = "depth-image-attachemnt-view",
    });
}

static vs_params_t compute_vsparams(float rx, float ry) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return (vs_params_t){ .mvp = vm_mul(model, view_proj) };
}

static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        int png_width, png_height, num_channels;
        const int desired_channels = 4;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &png_width, &png_height,
            &num_channels, desired_channels);
        if (pixels) {
            state.offscreen.png_img = sg_make_image(&(sg_image_desc){
                .width = png_width,
                .height = png_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .data.mip_levels[0] = {
                    .ptr = pixels,
                    .size = (size_t)(png_width * png_height * 4),
                },
                .label = "png-image",
            });
            stbi_image_free(pixels);

            // ...and initialize the pre-allocated texture view handle with that image
            sg_init_view(state.offscreen.png_tex_view, &(sg_view_desc){
                .texture = { .image = state.offscreen.png_img },
                .label = "png-texture-view",
            });
        }
    } else if (response->failed) {
        // if loading the file failed, set clear color to red
        state.offscreen.pass.action = (sg_pass_action) {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
        };
    }
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
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .window_title = "histogram-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
