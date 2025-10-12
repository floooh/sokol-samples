//------------------------------------------------------------------------------
//  drawex-sapp.c
//
//  Demonstrate/test sg_draw_ex() with base-vertex and base-index.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#include "sokol_color.h"
#include "dbgui/dbgui.h"
#include "drawex-sapp.glsl.h"

typedef enum {
    INVALID = 0,
    BASE_VERTEX_INSTANCE = (1 << 0),
    BASE_VERTEX = (1 << 1),
    BASE_INSTANCE = (1 << 2),
} draw_mode_t;

static struct {
    sg_pass_action pass_action;
    sg_buffer vtx_buf;
    sg_buffer idx_buf;
    sg_buffer inst_buf;
    sg_pipeline pip;
    uint8_t supported_modes; // bitmask of draw_mode_t
    draw_mode_t current_mode;
} state = {
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = SG_MAROON,
        }
    }
};

typedef struct { float x, y, r, g, b; } vertex_t;
typedef struct { float x, y, scale; } instance_t;

static void draw_panel(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_c64(),
        .logger.func = slog_func,
    });

    // create a bit mask of supported draw modes
    if (sg_query_features().draw_base_vertex) {
        state.supported_modes |= BASE_VERTEX;
    }
    if (sg_query_features().draw_base_instance) {
        state.supported_modes |= (BASE_INSTANCE | BASE_VERTEX_INSTANCE);
    }
    state.current_mode = BASE_VERTEX_INSTANCE & state.supported_modes;

    // vertex and index buffers with 2 separate sections (a triangle and a quad)
    const vertex_t vertices[7] = {
        // triangle
        { .x=0.0f,   .y=0.55f, .r=1.0f, .g=1.0f, .b=0.0f },
        { .x=0.25f,  .y=0.05f, .r=0.0f, .g=1.0f, .b=1.0f },
        { .x=-0.25f, .y=0.05f, .r=1.0f, .g=0.0f, .b=1.0f },

        // quad
        { .x=-0.25f, .y=-0.05f, .r=0.5f, .g=0.0f, .b=1.0f },
        { .x=0.25f,  .y=-0.05f, .r=0.5f, .g=1.0f, .b=0.0f },
        { .x=0.25f,  .y=-0.55f, .r=0.0f, .g=1.0f, .b=0.5f },
        { .x=-0.25f, .y=-0.55f, .r=1.0f, .g=0.0f, .b=0.5f },
    };
    uint16_t indices[9] = {
        // triangle
        0, 1, 2,
        // quad
        0, 1, 2, 0, 2, 3,
    };
    state.vtx_buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.vertex_buffer = true,
        .data = SG_RANGE(vertices),
        .label = "vertex-buffer",
    });
    state.idx_buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "index-buffer",
    });

    // a buffer with two sections of instance positions
    const float off_y = -0.2f;
    const float dx = 0.3f;
    const float dy = 0.4f;
    const float s = 0.4f;
    const instance_t instances[6] = {
        // first group
        { .x = -dx, .y = -dy   + off_y, .scale = s },
        { .x =  dx, .y =  0.0f + off_y, .scale = s },
        { .x = -dx, .y =  dy   + off_y, .scale = s },
        // second group
        { .x =  dx, .y = -dy   + off_y, .scale = s },
        { .x = -dx, .y =  0.0f + off_y, .scale = s },
        { .x =  dx, .y =  dy   + off_y, .scale = s },
    };
    state.inst_buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.vertex_buffer = true,
        .data = SG_RANGE(instances),
        .label = "instance-buffer",
    });

    // a shader and pipeline to render instanced 2D shapes
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(drawex_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [ATTR_drawex_pos] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 0 },
                [ATTR_drawex_color0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                [ATTR_drawex_inst_data] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
            },
        },
        .label = "render-pipeline",
    });
}

static void frame(void) {
    // draw the text panel via sokol-debugtext
    draw_panel();

    // actual render pass
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain_next() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers = {
            [0] = state.vtx_buf,
            [1] = state.inst_buf,
        },
        .index_buffer = state.idx_buf,
    });

    const int tri_base_index = 0;
    const int tri_num_indices = 3;
    const int quad_base_index = 3;
    const int quad_num_indices = 6;
    const int num_instances = 3;
    const int tri_base_vertex = 0;
    const int quad_base_vertex = 3;
    const int left_inst_base = 0;
    const int right_inst_base = 3;

    switch (state.current_mode) {
        case BASE_VERTEX:
            // draw with base_instance = 0
            sg_draw_ex(tri_base_index, tri_num_indices, num_instances, tri_base_vertex, left_inst_base);
            sg_draw_ex(quad_base_index, quad_num_indices, num_instances, quad_base_vertex, left_inst_base);
            break;
        case BASE_INSTANCE:
            // draw with base_vertex = 0
            sg_draw_ex(tri_base_index, tri_num_indices, num_instances, tri_base_vertex, left_inst_base);
            sg_draw_ex(tri_base_index, tri_num_indices, num_instances, tri_base_vertex, right_inst_base);
            break;
        case BASE_VERTEX_INSTANCE:
            sg_draw_ex(tri_base_index, tri_num_indices, num_instances, tri_base_vertex, left_inst_base);
            sg_draw_ex(quad_base_index, quad_num_indices, num_instances, quad_base_vertex, left_inst_base);
            sg_draw_ex(tri_base_index, tri_num_indices, num_instances, tri_base_vertex, right_inst_base);
            sg_draw_ex(quad_base_index, quad_num_indices, num_instances, quad_base_vertex, right_inst_base);
            break;
        default:
            break;
    }
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        switch (ev->key_code) {
            case SAPP_KEYCODE_1: state.current_mode = BASE_VERTEX; break;
            case SAPP_KEYCODE_2: state.current_mode = BASE_INSTANCE; break;
            case SAPP_KEYCODE_3: state.current_mode = BASE_VERTEX_INSTANCE; break;
            default: break;
        }
    }
    state.current_mode &= state.supported_modes;
    __dbgui_event(ev);
}

static void cleanup(void) {
    sdtx_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

static void draw_panel(void) {
    sdtx_canvas(sapp_width() * 0.5f, sapp_height() * 0.5f);
    sdtx_origin(1.0f, 2.0f);
    sdtx_home();
    if (state.supported_modes != 0) {
        sdtx_printf("Press keys 1,2,3:\n\n");
    }
    if (state.supported_modes & BASE_VERTEX) {
        sdtx_printf("%c (1) draw with base vertex\n", (state.current_mode == BASE_VERTEX) ? '*':' ');
    } else {
        sdtx_puts("!!! base vertex not supported\n");
    }
    if (state.supported_modes & BASE_INSTANCE) {
        sdtx_printf("%c (2) draw with base instance\n", (state.current_mode == BASE_INSTANCE) ? '*':' ');
    } else {
        sdtx_puts("!!! base instance not supported\n");
    }
    if (state.supported_modes & BASE_VERTEX_INSTANCE) {
        sdtx_printf("%c (3) draw with base vertex + base instance\n", (state.current_mode == BASE_VERTEX_INSTANCE) ? '*':' ');
    } else {
        sdtx_puts("!!! base vertex + base instance not supported\n");
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "drawex-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
