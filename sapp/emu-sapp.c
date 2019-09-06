//------------------------------------------------------------------------------
//  emu-sapp.c
//  A silly little emulator sample using some interesting visualizations.
//  Demonstrates use of a vertex-shader texture.
//
//  Insert coins with "1", start game with anything "any key" (except space).
//  "Push button" is spacebar.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_audio.h"
#include "dbgui/dbgui.h"
#include "emu/emu.h"
#include "emu-sapp.glsl.h"

#define SAMPLE_COUNT (4)

typedef struct {
    float x, y;
} vertex_t;

#define VERTS_X (EMU_WIDTH)
#define VERTS_Y (EMU_HEIGHT)

struct {
    emu_t emu;
    sg_pass_action pass_action;
    sg_bindings bind;
    sg_pipeline pip;
    float rx, ry;
    vertex_t vertices[VERTS_Y][VERTS_X];
    uint16_t indices[VERTS_Y * VERTS_X * 6];
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });
    __dbgui_setup(SAMPLE_COUNT);
    saudio_setup(&(saudio_desc){0});

    emu_setup(&state.emu);

    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    const float dx = 1.0f / VERTS_X;
    const float dy = 1.0f / VERTS_Y;
    float y = 0.0f;
    for (int iy = 0; iy < VERTS_Y; iy++, y+=dy) {
        float x = 0.0f;
        for (int ix = 0; ix < VERTS_X; ix++, x+=dx) {
            state.vertices[iy][ix].x = x;
            state.vertices[iy][ix].y = y;
        }
    }
    for (int iy = 0; iy < (VERTS_Y-1); iy++) {
        for (int ix = 0; ix < (VERTS_X-1); ix++) {
            int i_base = (ix + iy * VERTS_X) * 6;
            int v_base = ix + iy * VERTS_X;
            state.indices[i_base + 0] = v_base + 0;
            state.indices[i_base + 1] = v_base + 1;
            state.indices[i_base + 2] = v_base + VERTS_X;
            state.indices[i_base + 3] = v_base + 1;
            state.indices[i_base + 4] = v_base + VERTS_X;
            state.indices[i_base + 5] = v_base + VERTS_X + 1;
        }
    }
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .content = state.vertices,
        .size = sizeof(state.vertices),
    });
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .content = state.indices,
        .size = sizeof(state.indices),
    });

    state.bind.fs_images[0] = sg_make_image(&(sg_image_desc){
        .width = state.emu.width,
        .height = state.emu.height,
        .usage = SG_USAGE_STREAM,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });
    state.bind.vs_images[0] = state.bind.fs_images[0];

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT2,
            }
        },
        .shader = sg_make_shader(emu_shader_desc()),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLES,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_NONE,
            .sample_count = SAMPLE_COUNT
        },
    });

    state.rx = -35.0f;
}

static void frame(void) {

    hmm_mat4 proj = HMM_Perspective(90.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 2.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
//    state.rx += 0.1f; state.ry += 0.2f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    emu_frame(&state.emu, 16.667);
    sg_update_image(state.bind.fs_images[0], &(sg_image_content){
        .subimage[0][0] = {
            .ptr = state.emu.pixels,
            .size = state.emu.width * state.emu.height * sizeof(uint32_t)
        }
    });

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &vs_params, sizeof(vs_params));
    sg_draw(0, VERTS_X*VERTS_Y*6, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    emu_input(&state.emu, ev);
}

static void cleanup(void) {
    emu_shutdown(&state.emu);
    saudio_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 512,
        .height = 512,
        .sample_count = SAMPLE_COUNT,
        .gl_force_gles2 = true,
        .window_title = "Arcade Machine"
    };
}
