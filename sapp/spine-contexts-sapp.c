//------------------------------------------------------------------------------
//  spine-contexts-sapp.c
//  Test/demonstrate Spine rendering into sokol-gfx offscreen passes
//  via sokol_spine.h contexts.
//------------------------------------------------------------------------------
#define SOKOL_SPINE_IMPL
#define SOKOL_GL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "spine/spine.h"
#include "sokol_spine.h"
#include "sokol_gl.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "stb/stb_image.h"
#include "util/fileutil.h"
#include "dbgui/dbgui.h"

typedef struct {
    sspine_context ctx;
    sg_image img;
    sg_pass pass;
    sg_pass_action pass_action;
} offscreen_t;

typedef struct {
    bool loaded;
    sspine_range data;
} load_status_t;

static struct {
    offscreen_t offscreen[2];
    sg_sampler smp;
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instances[2];
    sspine_layer_transform layer_transform;
    double angle_deg;
    struct {
        load_status_t atlas;
        load_status_t skeleton;
        bool failed;
    } load_status;
    struct {
        uint8_t atlas[16 * 1024];
        uint8_t skeleton[512 * 1024];
        uint8_t image[512 * 1024];
    } buffers;
} state;

static offscreen_t setup_offscreen(sg_pixel_format fmt, int width_height, sg_color clear_color);
static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void create_spine_objects(void);
typedef struct {
    struct { float x; float y; } pos;
    struct { float x; float y; } scale;
    float rot;
    sg_image img;
    sg_sampler smp;
} quad_params_t;
static void draw_quad(quad_params_t params);

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func
    });
    sspine_setup(&(sspine_desc){
        .logger.func = slog_func
    });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // create 2 sspine contexts for rendering into offscreen render targets
    state.offscreen[0] = setup_offscreen(SG_PIXELFORMAT_RGBA8, 512, (sg_color){ 1.0f, 1.0f, 1.0f, 1.0f });
    state.offscreen[1] = setup_offscreen(SG_PIXELFORMAT_RG8, 64, (sg_color){ 1.0f, 1.0f, 1.0f, 1.0f });

    // create a texture sampler for sampling the offscreen render targets as texture
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // common fixed-size spine layer transform
    state.layer_transform = (sspine_layer_transform){
        .size = { .x = 512.0f, .y = 512.0f },
        .origin = { .x = 256.0f, .y = 256.0f },
    };

    // start loading spine atlas and skeleton
    char path[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("speedy-pma.atlas", path, sizeof(path)),
        .channel = 0,
        .buffer = SFETCH_RANGE(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("speedy-ess.skel", path, sizeof(path)),
        .channel = 1,
        .buffer = SFETCH_RANGE(state.buffers.skeleton),
        .callback = skeleton_data_loaded,
    });
}

static void frame(void) {
    const float delta_time = (float)sapp_frame_duration();
    sfetch_dowork();

    // render spine objects in separate contexts, first one by setting the current context,
    // second one by calling function with ctx arg
    sspine_update_instance(state.instances[0], delta_time);
    sspine_set_context(state.offscreen[0].ctx);
    sspine_draw_instance_in_layer(state.instances[0], 0);

    sspine_update_instance(state.instances[1], delta_time);
    sspine_context_draw_instance_in_layer(state.offscreen[1].ctx, state.instances[1], 0);

    // draw two quads via sokol-gl which use the offscreen-rendered spine scenes as textures
    const float dw = sapp_widthf();
    const float dh = sapp_heightf();
    const float aspect = dh / dw;
    state.angle_deg += sapp_frame_duration() * 60.0;
    sgl_defaults();
    sgl_enable_texture();
    sgl_matrix_mode_projection();
    sgl_ortho(-1.0f, +1.0f, aspect, -aspect, -1.0f, +1.0f);
    sgl_matrix_mode_modelview();
    draw_quad((quad_params_t){
        .pos = { -0.425f, 0.0f },
        .scale = { 0.4f, 0.4f },
        .rot = sgl_rad((float)state.angle_deg),
        .img = state.offscreen[0].img,
        .smp = state.smp,
    });
    draw_quad((quad_params_t){
        .pos = { +0.425f, 0.0f },
        .scale = { 0.4f, 0.4f },
        .rot = -sgl_rad((float)state.angle_deg),
        .img = state.offscreen[1].img,
        .smp = state.smp,
    });

    // do all the sokol-gfx rendering:
    // - two offscreen pass, rendering spine jnstances into render target textures
    //   (one is setting the current spine context, other is calling draw function with ctx arg)
    // - the default pass which renders the previously recorded sokol-gl scene using
    //   the two render targets as textures
    //
    sg_begin_pass(state.offscreen[0].pass, &state.offscreen[0].pass_action);
    sspine_set_context(state.offscreen[0].ctx);
    sspine_draw_layer(0, &state.layer_transform);
    sg_end_pass();

    sg_begin_pass(state.offscreen[1].pass, &state.offscreen[1].pass_action);
    sspine_context_draw_layer(state.offscreen[1].ctx, 0, &state.layer_transform);
    sg_end_pass();

    sg_begin_default_pass(&(sg_pass_action){0}, sapp_width(), sapp_height());
    sgl_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sfetch_shutdown();
    sspine_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

// helper function to create an offscreen render target and pass, and matching sokol-spine context
offscreen_t setup_offscreen(sg_pixel_format fmt, int width_height, sg_color clear_color) {
    const sg_image img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = width_height,
        .height = width_height,
        .pixel_format = fmt,
        .sample_count = 1,
    });
    return (offscreen_t){
        .ctx = sspine_make_context(&(sspine_context_desc){
            .color_format = fmt,
            .depth_format = SG_PIXELFORMAT_NONE,
            .sample_count = 1,
        }),
        .img = img,
        .pass = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0] = {
                .image = img,
            }
        }),
        .pass_action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = clear_color },
        },
    };
}

// fetch callback for atlas data
static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.atlas = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ .ptr = response->data.ptr, .size = response->data.size }
        };
        // when both atlas and skeleton data loaded, create spine objects
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// fetch callback for skeleton data
static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.skeleton = (load_status_t){
            .loaded = true,
            .data = (sspine_range){ .ptr = response->data.ptr, .size = response->data.size }
        };
        // when both atlas and skeleton data loaded, create spine objects
        if (state.load_status.atlas.loaded && state.load_status.skeleton.loaded) {
            create_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// called when both the atlas and skeleton files have been loaded,
// creates spine atlas, skeleton and instance, and starts loading
// the atlas image(s)
static void create_spine_objects(void) {
    // create spine atlas object
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = state.load_status.atlas.data
    });
    assert(sspine_atlas_valid(state.atlas));

    // create spine skeleton object
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .anim_default_mix = 0.2f,
        .binary_data = state.load_status.skeleton.data,
    });
    assert(sspine_skeleton_valid(state.skeleton));

    // create two instance objects
    for (int i = 0; i < 2; i++) {
        state.instances[i] = sspine_make_instance(&(sspine_instance_desc){
            .skeleton = state.skeleton
        });
        assert(sspine_instance_valid(state.instances[i]));
        sspine_set_position(state.instances[i], (sspine_vec2){ 0.0f, 128.0f });
        sspine_set_animation(state.instances[i], sspine_anim_by_name(state.skeleton, (i & 1) ? "run-linear" : "run"), 0, true);
    }

    // start loading atlas images
    const int num_images = sspine_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image img = sspine_image_by_index(state.atlas, img_index);
        const sspine_image_info img_info = sspine_get_image_info(img);
        assert(img_info.valid);
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .channel = 0,
            .path = fileutil_get_path(img_info.filename.cstr, path_buf, sizeof(path_buf)),
            .buffer = SFETCH_RANGE(state.buffers.image),
            .callback = image_data_loaded,
            .user_data = SFETCH_RANGE(img),
        });
    }
}

// load spine atlas image data and create a sokol-gfx image object
static void image_data_loaded(const sfetch_response_t* response) {
    const sspine_image img = *(sspine_image*)response->user_data;
    const sspine_image_info img_info = sspine_get_image_info(img);
    assert(img_info.valid);
    if (response->fetched) {
        // decode pixels via stb_image.h
        const int desired_channels = 4;
        int img_width, img_height, num_channels;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &img_width,
            &img_height,
            &num_channels, desired_channels);
        if (pixels) {
            // sokol-spine has already allocated a sokol-gfx image and sampler handle for us,
            // now "populate" the handles with an actual image and sampler
            sg_init_image(img_info.sgimage, &(sg_image_desc){
                .width = img_width,
                .height = img_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .label = img_info.filename.cstr,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(img_width * img_height * 4)
                }
            });
            sg_init_sampler(img_info.sgsampler, &(sg_sampler_desc){
                .min_filter = img_info.min_filter,
                .mag_filter = img_info.mag_filter,
                .mipmap_filter = img_info.mipmap_filter,
                .wrap_u = img_info.wrap_u,
                .wrap_v = img_info.wrap_v,
                .label = img_info.filename.cstr,
            });
            stbi_image_free(pixels);
        }
        else {
            state.load_status.failed = true;
            sg_fail_image(img_info.sgimage);
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
        sg_fail_image(img_info.sgimage);
    }
}

// draw a rotating quad via sokol-gl
static void draw_quad(quad_params_t params) {
    sgl_texture(params.img, params.smp);
    sgl_push_matrix();
    sgl_translate(params.pos.x, params.pos.y, 0.0f);
    sgl_scale(params.scale.x, params.scale.y, 0.0f);
    sgl_rotate(params.rot, 0.0f, 0.0f, 1.0f);
    sgl_begin_quads();
    sgl_v2f_t2f(-1.0f, -1.0f, 0.0f, 0.0f);
    sgl_v2f_t2f(+1.0f, -1.0f, 1.0f, 0.0f);
    sgl_v2f_t2f(+1.0f, +1.0f, 1.0f, 1.0f);
    sgl_v2f_t2f(-1.0f, +1.0f, 0.0f, 1.0f);
    sgl_end();
    sgl_pop_matrix();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 1024,
        .height = 768,
        .sample_count = 4,
        .window_title = "spine-contexts-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
