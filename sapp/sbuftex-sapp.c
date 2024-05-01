//------------------------------------------------------------------------------
//  sbuftex-sapp.c
//
//  Test whether binding texture and storage buffers to the same shader stage
//  works.
//
//  The vertex shader pulls vertices and a color palette index from a storage
//  buffer. The color palette index is passed to the fragment shader.
//
//  In the fragment shader, a color palette lookup is done via a storage buffer,
//  and the resulting color is modulated by a texture.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "sbuftex-sapp.glsl.h"

static struct {
    float rx, ry;
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
} state;

static vs_params_t make_vs_params(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // if no storage buffers supported, just clear the screen to red
    if (!sg_query_features().storage_buffer) {
        state.pass_action = (sg_pass_action){
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } },
        };
        return;
    }
    // otherwise clear to a regular color
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.75f, 0.25f } },
    };

    // a storage buffer with cube vertices
    sb_vertex_t vertices[] = {
        { .pos = { -1.0f, -1.0f, -1.0f },  .idx = 0, .uv = { 0.0f, 0.0f } },
        { .pos = {  1.0f, -1.0f, -1.0f },  .idx = 0, .uv = { 1.0f, 0.0f } },
        { .pos = {  1.0f,  1.0f, -1.0f },  .idx = 0, .uv = { 1.0f, 1.0f } },
        { .pos = { -1.0f,  1.0f, -1.0f },  .idx = 0, .uv = { 0.0f, 1.0f } },

        { .pos = { -1.0f, -1.0f,  1.0f },  .idx = 1, .uv = { 0.0f, 0.0f } },
        { .pos = {  1.0f, -1.0f,  1.0f },  .idx = 1, .uv = { 1.0f, 0.0f } },
        { .pos = {  1.0f,  1.0f,  1.0f },  .idx = 1, .uv = { 1.0f, 1.0f } },
        { .pos = { -1.0f,  1.0f,  1.0f },  .idx = 1, .uv = { 0.0f, 1.0f } },

        { .pos = { -1.0f, -1.0f, -1.0f },  .idx = 2, .uv = { 0.0f, 0.0f } },
        { .pos = { -1.0f,  1.0f, -1.0f },  .idx = 2, .uv = { 1.0f, 0.0f } },
        { .pos = { -1.0f,  1.0f,  1.0f },  .idx = 2, .uv = { 1.0f, 1.0f } },
        { .pos = { -1.0f, -1.0f,  1.0f },  .idx = 2, .uv = { 0.0f, 1.0f } },

        { .pos = {  1.0f, -1.0f, -1.0f },  .idx = 3, .uv = { 0.0f, 0.0f } },
        { .pos = {  1.0f,  1.0f, -1.0f },  .idx = 3, .uv = { 1.0f, 0.0f } },
        { .pos = {  1.0f,  1.0f,  1.0f },  .idx = 3, .uv = { 1.0f, 1.0f } },
        { .pos = {  1.0f, -1.0f,  1.0f },  .idx = 3, .uv = { 0.0f, 1.0f } },

        { .pos = { -1.0f, -1.0f, -1.0f },  .idx = 4, .uv = { 0.0f, 0.0f } },
        { .pos = { -1.0f, -1.0f,  1.0f },  .idx = 4, .uv = { 1.0f, 0.0f } },
        { .pos = {  1.0f, -1.0f,  1.0f },  .idx = 4, .uv = { 1.0f, 1.0f } },
        { .pos = {  1.0f, -1.0f, -1.0f },  .idx = 4, .uv = { 0.0f, 1.0f } },

        { .pos = { -1.0f,  1.0f, -1.0f },  .idx = 5, .uv = { 0.0f, 0.0f } },
        { .pos = { -1.0f,  1.0f,  1.0f },  .idx = 5, .uv = { 1.0f, 0.0f } },
        { .pos = {  1.0f,  1.0f,  1.0f },  .idx = 5, .uv = { 1.0f, 1.0f } },
        { .pos = {  1.0f,  1.0f, -1.0f },  .idx = 5, .uv = { 0.0f, 1.0f } },
    };
    state.bind.vs.storage_buffers[SLOT_vertices] = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_STORAGEBUFFER,
        .data = SG_RANGE(vertices),
        .label = "cube-vertices",
    });

    // an index buffer with the cube indices
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "cube-indices",
    });

    // a storage buffer with a color palette
    sb_color_t colors[] = {
        { .color = { 1.0f, 0.0f, 0.0f, 1.0f } },
        { .color = { 0.0f, 1.0f, 0.0f, 1.0f } },
        { .color = { 0.0f, 0.0f, 1.0f, 1.0f } },
        { .color = { 0.5f, 0.0f, 1.0f, 1.0f } },
        { .color = { 0.0f, 0.5f, 1.0f, 1.0f } },
        { .color = { 1.0f, 0.5f, 0.0f, 1.0f } },
    };
    state.bind.fs.storage_buffers[SLOT_colors] = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_STORAGEBUFFER,
        .data = SG_RANGE(colors),
        .label = "color-palette",
    });

    // a greyscale texture which modulates the color palette colors
    uint8_t pixels[4][4] = {
        { 0xFF, 0xCC, 0x88, 0x44 },
        { 0xCC, 0x88, 0x44, 0xFF },
        { 0x88, 0x44, 0xFF, 0xCC },
        { 0x44, 0xFF, 0xCC, 0x88 },
    };
    state.bind.fs.images[SLOT_tex] = sg_make_image(&(sg_image_desc){
        .width = 4,
        .height = 4,
        .pixel_format = SG_PIXELFORMAT_R8,
        .data.subimage[0][0] = SG_RANGE(pixels),
        .label = "texture",
    });

    // ...and a matching sampler
    state.bind.fs.samplers[SLOT_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "sampler",
    });

    // ...shader and pipeline object (note: no vertex layout needed because
    // the vertex shader pulls vertices from storage buffer)
    const sg_shader shd = sg_make_shader(sbuftex_shader_desc(sg_query_backend()));
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "pipeline",
    });
}

static void frame(void) {
    const vs_params_t vs_params = make_vs_params();
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    if (sg_query_features().storage_buffer) {
        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static vs_params_t make_vs_params(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    hmm_mat4 proj = HMM_Perspective(60.0f, sapp_widthf()/sapp_heightf(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    return (vs_params_t){ .mvp = HMM_MultiplyMat4(view_proj, model) };
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
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
        .window_title = "sbuftex-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
