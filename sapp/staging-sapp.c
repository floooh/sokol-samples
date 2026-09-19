//------------------------------------------------------------------------------
//  staging-sapp.c
//
//  Demonstrate uploading data into vertex- and index-buffer segments
//  via a write-transient staging buffer and a buffer-to-buffer GPU copy
//  operation.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#include "dbgui/dbgui.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "staging-sapp.glsl.h"
#include <assert.h>

#define NUM_SEGMENTS (6)
#define MAX_SEGMENT_VERTICES (4096)
#define MAX_SEGMENT_INDICES (MAX_SEGMENT_VERTICES * 3)
#define SHAPE_CHANGE_INTERVAL_SEC (1.0)

static struct {
    sg_buffer vertex_buffer;
    sg_buffer index_buffer;
    sg_buffer staging_buffer;
    sg_pipeline pip;
    sg_pass_action pass_action;
    int vtx_segment_size;
    int idx_segment_size;
    double shape_change_tracker;
    float rx, ry;
    sshape_element_range_t shapes[NUM_SEGMENTS];
} state = {
    .pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.1f, 0.1f, 0.15f, 1.0f },
        },
    },
};

static mat44_t compute_mvp(double dt);
static void update_random_segment(void);
static void apply_segment_viewport(uint32_t seg);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    const sshape_optional_components_t vtx_comps = { .colors = true };

    // a single 'staging buffer' for enough room
    state.vtx_segment_size = MAX_SEGMENT_VERTICES * sshape_vertex_size(&vtx_comps);
    state.idx_segment_size = MAX_SEGMENT_INDICES * sizeof(uint16_t);
    state.staging_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .write_transient = true,
            .copy_src = true,
        },
        .size = (size_t)(state.vtx_segment_size + state.idx_segment_size),
        .label = "staging-buffer",
    });

    // a vertex buffer with enough room for all dynamically updated 'segments'
    state.vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .vertex_buffer = true,  // technically not needed, since it's the default
            .copy_dst = true,
        },
        .size = (size_t)(NUM_SEGMENTS * state.vtx_segment_size),
        .label = "vertex-buffer",
    });

    // ...and an index buffer with the same 'segmentation'
    state.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .index_buffer = true,
            .copy_dst = true,
        },
        .size = (size_t)(NUM_SEGMENTS * state.idx_segment_size),
        .label = "index-buffer",
    });

    // pipeline object for rendering 3D shapes
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(shape_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_shape_position].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_shape_color0].format = SG_VERTEXFORMAT_UBYTE4N,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .label = "shape-pipeline"
    });
}

static void frame(void) {
    const double dt = sapp_frame_duration();

    // time to update one of the shape segments?
    state.shape_change_tracker += dt;
    if (state.shape_change_tracker > SHAPE_CHANGE_INTERVAL_SEC) {
        state.shape_change_tracker -= SHAPE_CHANGE_INTERVAL_SEC;
        update_random_segment();
    }

    const vs_params_t vs_params = { .mvp = compute_mvp(dt) };
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    for (uint32_t seg = 0; seg < NUM_SEGMENTS; seg++) {
        // current buffer segment already occupied?
        if (state.shapes[seg].num_elements > 0) {
            apply_segment_viewport(seg);
            sg_apply_bindings(&(sg_bindings){
                .vertex_buffers[0] = state.vertex_buffer,
                .vertex_buffer_offsets[0] = seg * state.vtx_segment_size,
                .index_buffer = state.index_buffer,
                .index_buffer_offset = seg * state.idx_segment_size,
            });
            sg_draw(state.shapes[seg].base_element, state.shapes[seg].num_elements, 1);
        }
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

static void apply_segment_viewport(uint32_t seg) {
    int row = (int)(seg & 1);
    int col = (int)(seg >> 1);
    int seg_h = sapp_height() / 2;
    int seg_w = sapp_width() / (NUM_SEGMENTS / 2);
    int seg_x = col * seg_w;
    int seg_y = row * seg_h;
    sg_apply_viewport(seg_x, seg_y, seg_w, seg_h, true);
}

static mat44_t compute_mvp(double dt) {
    const float t = (float)(dt * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(state.rx));
    mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, view_proj);
}

static uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static void update_random_segment(void) {
    // random segment index to update
    uint32_t seg = xorshift32() % NUM_SEGMENTS;
    // random shape type
    uint32_t shape_type = xorshift32() % 4;

    // build shape vertex- and index-data
    static uint8_t vertices[SSHAPE_MAX_VERTEX_SIZE * MAX_SEGMENT_VERTICES];
    static uint16_t indices[MAX_SEGMENT_INDICES];
    sshape_state_t shp = {
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer = SSHAPE_RANGE(indices),
        .disable = {
            .normals = true,
            .texcoords = true,
        },
    };
    switch (shape_type) {
        case 0:
            sshape_build_box(&shp, &(sshape_box_t){
                .width = 1.0f,
                .height = 1.0f,
                .depth = 1.0f,
                .tiles = 10,
                .random_colors = true,
            });
            break;
        case 1:
            sshape_build_sphere(&shp, &(sshape_sphere_t){
                .radius = 0.75f,
                .slices = 36,
                .stacks = 20,
                .random_colors = true,
            });
            break;
        case 2:
            sshape_build_cylinder(&shp, &(sshape_cylinder_t){
                .radius = 0.5f,
                .height = 1.5f,
                .slices = 36,
                .stacks = 10,
                .random_colors = true,
            });
            break;
        default:
            sshape_build_torus(&shp, &(sshape_torus_t) {
                .radius = 0.5f,
                .ring_radius = 0.3f,
                .rings = 36,
                .sides = 18,
                .random_colors = true,
            });
            break;
    }
    assert(shp.valid);
    assert(shp.vertices.data_size <= (size_t)state.vtx_segment_size);
    assert(shp.indices.data_size <= (size_t)state.idx_segment_size);
    state.shapes[seg] = sshape_element_range(&shp);

    // first write both shape vertex- and index-data into common
    // write-transient staging buffer
    const sg_range vtx_data = sshape_vertex_buffer_desc(&shp).data;
    const sg_range idx_data = sshape_index_buffer_desc(&shp).data;
    sg_write_buffer_transient(&(sg_write_buffer_desc){
        .src.data = vtx_data,
        .dst = {
            .buffer = state.staging_buffer,
            .offset = 0,
        },
    });
    sg_write_buffer_transient(&(sg_write_buffer_desc){
        .src.data = idx_data,
        .dst = {
            .buffer = state.staging_buffer,
            .offset = state.vtx_segment_size,
        },
    });

    // then 'persist' the data by copying into a specific segment
    // in the vertex- and index-buffer
    sg_copy_buffer_to_buffer(&(sg_copy_buffer_to_buffer_desc){
        .src = {
            .buffer = state.staging_buffer,
            .offset = 0,
        },
        .dst = {
            .buffer = state.vertex_buffer,
            .offset = (size_t)(seg * state.vtx_segment_size),
        },
        .size = vtx_data.size,
    });
    sg_copy_buffer_to_buffer(&(sg_copy_buffer_to_buffer_desc){
        .src = {
            .buffer = state.staging_buffer,
            .offset = state.vtx_segment_size,
        },
        .dst = {
            .buffer = state.index_buffer,
            .offset = (size_t)(seg * state.idx_segment_size),
        },
        // NOTE: only the offsets must be 4-byte-aligned,
        // odd sizes will be rounded up automatically
        .size = idx_data.size,
    });
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
        .window_title = "staging-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
