//------------------------------------------------------------------------------
//  loadpng-sapp.c
//  Asynchronously load a png file via sokol_fetch.h, decode via stb_image.h
//  (this is non-perfect since it happens on the main thread)
//  and create a sokol-gfx texture from the decoded pixel data.
//
//  The CMakeLists.txt entry for loadpng-sapp.c also demonstrates the
//  sokol_file_copy() macro to copy assets into the fips deployment directory.
//
//  This is a modified version of texcube-sapp.c
//------------------------------------------------------------------------------
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "stb/stb_image.h"
#include "dbgui/dbgui.h"
#include "util/fileutil.h"
#include "loadpng-sapp.glsl.h"

static struct {
    float rx, ry;
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    uint8_t file_buffer[256 * 1024];
} state;

typedef struct {
    float x, y, z;
    int16_t u, v;
} vertex_t;

static vs_params_t compute_vsparams(float rx, float ry);
static void fetch_callback(const sfetch_response_t*);

static void init(void) {
    // setup sokol-gfx and the optional debug-ui
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-fetch with the minimal "resource limits"
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 1,
        .num_channels = 1,
        .num_lanes = 1,
        .logger.func = slog_func,
    });

    // pass action for clearing the framebuffer to some color
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.125f, 0.25f, 0.35f, 1.0f } }
    };

    /* Allocate a view handle, but don't actually initialize it yet,
       this happens later when the asynchronous file load has finished.
       Any draw calls containing such an "incomplete" view handle
       will be silently dropped.
    */
    state.bind.views[VIEW_tex] = sg_alloc_view();

    // a sampler object
    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .label = "png-sampler",
    });

    // cube vertex buffer with packed texcoords
    const vertex_t vertices[] = {
        // pos                  uvs
        { -1.0f, -1.0f, -1.0f,      0,     0 },
        {  1.0f, -1.0f, -1.0f,  32767,     0 },
        {  1.0f,  1.0f, -1.0f,  32767, 32767 },
        { -1.0f,  1.0f, -1.0f,      0, 32767 },

        { -1.0f, -1.0f,  1.0f,      0,     0 },
        {  1.0f, -1.0f,  1.0f,  32767,     0 },
        {  1.0f,  1.0f,  1.0f,  32767, 32767 },
        { -1.0f,  1.0f,  1.0f,      0, 32767 },

        { -1.0f, -1.0f, -1.0f,      0,     0 },
        { -1.0f,  1.0f, -1.0f,  32767,     0 },
        { -1.0f,  1.0f,  1.0f,  32767, 32767 },
        { -1.0f, -1.0f,  1.0f,      0, 32767 },

        {  1.0f, -1.0f, -1.0f,      0,     0 },
        {  1.0f,  1.0f, -1.0f,  32767,     0 },
        {  1.0f,  1.0f,  1.0f,  32767, 32767 },
        {  1.0f, -1.0f,  1.0f,      0, 32767 },

        { -1.0f, -1.0f, -1.0f,      0,     0 },
        { -1.0f, -1.0f,  1.0f,  32767,     0 },
        {  1.0f, -1.0f,  1.0f,  32767, 32767 },
        {  1.0f, -1.0f, -1.0f,      0, 32767 },

        { -1.0f,  1.0f, -1.0f,      0,     0 },
        { -1.0f,  1.0f,  1.0f,  32767,     0 },
        {  1.0f,  1.0f,  1.0f,  32767, 32767 },
        {  1.0f,  1.0f, -1.0f,      0, 32767 },
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });

    // create an index buffer for the cube
    const uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(loadpng_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_loadpng_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_loadpng_texcoord0].format = SG_VERTEXFORMAT_SHORT2N
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .label = "cube-pipeline"
    });

    /* start loading the PNG file, we don't need the returned handle since
       we can also get that inside the fetch-callback from the response
       structure.
        - NOTE that we're not using the user_data member, since all required
          state is in a global variable anyway
    */
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("baboon.png", path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer = SFETCH_RANGE(state.file_buffer)
    });
}

/* The frame-function is fairly boring, note that no special handling is
   needed for the case where the texture isn't loaded yet.
   Also note the sfetch_dowork() function, this is usually called once a
   frame to pump the sokol-fetch message queues.
*/
static void frame(void) {
    // pump the sokol-fetch message queues, and invoke response callbacks
    sfetch_dowork();

    // compute model-view-projection matrix for vertex shader
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const vs_params_t vs_params = compute_vsparams(state.rx, state.ry);

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

/* The fetch-callback is called by sokol_fetch.h when the data is loaded,
   or when an error has occurred.
*/
static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        /* the file data has been fetched, since we provided a big-enough
           buffer we can be sure that all data has been loaded here
        */
        int png_width, png_height, num_channels;
        const int desired_channels = 4;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &png_width, &png_height,
            &num_channels, desired_channels);
        if (pixels) {
            // create an image object from the loaded texture date
            sg_image img = sg_make_image(&(sg_image_desc){
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
            sg_init_view(state.bind.views[VIEW_tex], &(sg_view_desc){
                .texture = { .image = img },
                .label = "png-texture-view",
            });
        }
    } else if (response->failed) {
        // if loading the file failed, set clear color to red
        state.pass_action = (sg_pass_action) {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
        };
    }
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

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "Async PNG Loading (sokol-app)",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
