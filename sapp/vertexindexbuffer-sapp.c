//------------------------------------------------------------------------------
//  vertexindexbuffer-sapp.c
//
//  A variant of cube-sapp which puts vertices and indices into the same
//  buffer (which isn't allowed on WebGL2, but fine on other APIs).
//------------------------------------------------------------------------------
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"
#include "vertexindexbuffer-sapp.glsl.h"

static struct {
    float rx, ry;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
} state;

static void draw_fallback(void);
static vs_params_t compute_vsparams(void);

static const float vertices[] = {
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

static const uint16_t indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // if combined vertex/index buffers are not supported, setup
    // sokol_debugtext.h to render an error message and return
    if (sg_query_features().separate_buffer_types) {
        sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_cpc(), .logger.func = slog_func });
        // set pass action to render to red
        state.pass_action = (sg_pass_action){
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1, 0, 0, 1 } },
        };
        return;
    }

    // setup pass action to clear to background color
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.375, 0.125, 0.25 } },
    };

    // create a buffer with both vertices and indices
    const size_t buf_size = sizeof(vertices) + sizeof(indices);
    const size_t indices_offset = sizeof(vertices);
    uint8_t* data_ptr = malloc(buf_size);
    memcpy(data_ptr, vertices, sizeof(vertices));
    memcpy(data_ptr + indices_offset, indices, sizeof(indices));
    sg_buffer buf = sg_make_buffer(&(sg_buffer_desc){
        // indicate that this buffer is going to be bound as vertex- and index-buffer
        .usage = {
            .vertex_buffer = true,
            .index_buffer = true,
        },
        .data = { .ptr = data_ptr, .size = buf_size },
        .label = "vertex-index-buffer",
    });
    free(data_ptr);

    // setup the resource bindings, the same buffer is bound as
    // vertex- and index-buffer, with the index data starting at an offset
    state.bind = (sg_bindings){
        .vertex_buffers[0] = buf,
        .index_buffer = buf,
        .index_buffer_offset = indices_offset,
    };

    // create pipeline object (nothing special here)
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(cube_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_cube_color0].format = SG_VERTEXFORMAT_FLOAT4,
            },
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .label = "render-pipeline",
    });
}

static void frame(void) {
    // draw fallback?
    if (sg_query_features().separate_buffer_types) {
        draw_fallback();
        return;
    }

    // otherwise render regular frame
    const vs_params_t vs_params = compute_vsparams();
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    if (sg_query_features().separate_buffer_types) {
        sdtx_shutdown();
    }
    __dbgui_shutdown();
    sg_shutdown();
}

static vs_params_t compute_vsparams(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const float t = (float)(sapp_frame_duration() * 60.0);
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    mat44_t rxm = mat44_rotation_x(vm_radians(state.rx));
    mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    mat44_t model = vm_mul(rym, rxm);
    return (vs_params_t){ .mvp = vm_mul(model, view_proj) };
}

static void draw_fallback(void) {
    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_pos(1, 1);
    sdtx_puts("WebGL2 doesn't support combined vertex/index buffers");
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    sdtx_draw();
    sg_end_pass();
    sg_commit();
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
        .window_title = "vertexindexbuffer-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
