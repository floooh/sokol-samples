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
        sg_pipeline draw_pip;
    } histogram;
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
        sg_image img;
        sg_view tex_view;
        sg_sampler smp;
    } cube;
    float rx, ry;
    bool load_failed;
} state;

static uint8_t file_buffer[256 * 1024];

static const float cube_vertices[] = {
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

static const uint16_t cube_indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

static vs_params_t compute_vsparams(void);
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

    // a storage buffer and view for the histogram data
    state.histogram.sbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .size = NUM_HISTOGRAM_BINS * sizeof(hist_bin_t),
        .label = "histogram-buffer",
    });
    state.histogram.sbuf_view = sg_make_view(&(sg_view_desc){
        .storage_buffer.buffer = state.histogram.sbuf,
        .label = "histogram-buffer-view",
    });

    // a shader and compute pipeline to clear the histogram buffer
    // at the start of each frame
    state.histogram.clear_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = sg_make_shader(clear_shader_desc(sg_query_backend())),
        .label = "histogram-clear-pipeline",
    });

    // everything needed for rendering a textured cube
    state.cube.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices),
        .label = "cube-vertices"
    });
    state.cube.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices),
        .label = "cube-indices"
    });
    state.cube.pip = sg_make_pipeline(&(sg_pipeline_desc){
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
        },
        .label = "cube-pipeline",
    });
    state.cube.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "sampler",
    });

    // image and texture view will be initialized once the texture file has been loaded
    state.cube.img = sg_alloc_image();
    state.cube.tex_view = sg_alloc_view();

    // a bufferless pipeline for rendering a single histogram bar
    state.histogram.draw_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_hist_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "display-histogram-pipeline",
    });

    // start loading the texture file
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("baboon.png", path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(file_buffer)
    });
}

static void frame(void) {
    sfetch_dowork();
    const vs_params_t vs_params = compute_vsparams();

    // a compute pass which clears the histogram storage buffer
    sg_begin_pass(&(sg_pass){ .compute = true });
    sg_apply_pipeline(state.histogram.clear_pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_hist_clear] = state.histogram.sbuf_view,
    });
    sg_dispatch(NUM_HISTOGRAM_BINS / WORKGROUP_WIDTH, 1, 1);
    sg_end_pass();

    // render the cube into the swapchain surfaces, this also collects histogram data
    // into the storage buffer
    // (NOTE: the texture may not be loaded yet, in that case the draw call
    // is automatically skipped)
    const sg_pass_action action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            // when loading the texture has failed, clear to red
            .clear_value = state.load_failed
                ? (sg_color){ 1.0f, 0.0f, 0.0f, 1.0f}
                : (sg_color){ 0.5f, 0.5f, 0.5f, 1.0f },
        },
    };
    const sg_swapchain swapchain = sglue_swapchain();
    sg_begin_pass(&(sg_pass){ .action = action, .swapchain = swapchain });
    sg_apply_pipeline(state.cube.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.cube.vbuf,
        .index_buffer = state.cube.ibuf,
        .views = {
            [VIEW_hist_out] = state.histogram.sbuf_view,
            [VIEW_shape_tex] = state.cube.tex_view,
        },
        .samplers[SMP_shape_smp] = state.cube.smp,
    });
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // rendering the histogram needs to happen in a new pass, because the
    // histogram storage buffer access changes from read/write in the fragment
    // stage to read-only in the vertex stage, this new pass continues rendering
    // into the swapchain surface though
    _dbgui_update();
    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0].load_action = SG_LOADACTION_LOAD,
            .depth.load_action = SG_LOADACTION_DONTCARE,
        },
        .swapchain = swapchain
    });
    // draw the three histogram bars
    const int w = sapp_width() - 2 * 20;
    const int x = 20;
    const int h = sapp_height() / 10;
    const int y0 = sapp_height() - 4 * h;
    sg_apply_pipeline(state.histogram.draw_pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_hist_in] = state.histogram.sbuf_view,
    });
    for (int i = 0; i < 3; i++) {
        const vs_hist_params_t vs_hist_params = {
            .channel = i,
        };
        sg_apply_viewport(x, y0 + i * h, w, h, true);
        sg_apply_uniforms(UB_vs_hist_params, &SG_RANGE(vs_hist_params));
        sg_draw(0, 4, NUM_HISTOGRAM_BINS-1);
    }
    _dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sfetch_shutdown();
    _dbgui_shutdown();
    sg_shutdown();
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
            sg_init_image(state.cube.img, &(sg_image_desc){
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
            sg_init_view(state.cube.tex_view, &(sg_view_desc){
                .texture = { .image = state.cube.img },
                .label = "png-texture-view",
            });
        } else {
            state.load_failed = true;
        }
    } else if (response->failed) {
        state.load_failed = true;
    }
}

static vs_params_t compute_vsparams(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(state.rx));
    mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    mat44_t model = vm_mul(rym, rxm);
    return (vs_params_t){ .mvp = vm_mul(model, view_proj), .mv = vm_mul(model, view) };
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = _dbgui_event,
        .width = 800,
        .height = 600,
        .window_title = "histogram-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
