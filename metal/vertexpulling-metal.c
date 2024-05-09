//------------------------------------------------------------------------------
//  vertexpulling-metal.c
//
//  Demonstrates vertex pulling from storage buffers.
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry;
} state;

typedef struct {
    float pos[4];
    float color[4];
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.5f, 1.0f, 1.0f } },
    };

    vertex_t vertices[] = {
        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.0, 1.0 } },

        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 1.0, 0.0, 1.0 } },

        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.0, 1.0, 1.0 } },

        { .pos = { 1.0, -1.0, -1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0,  1.0, -1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0,  1.0,  1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },
        { .pos = { 1.0, -1.0,  1.0, 1.0 }, .color = {1.0, 0.5, 0.0, 1.0, } },

        { .pos = { -1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = { -1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0,  1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },
        { .pos = {  1.0, -1.0, -1.0, 1.0 }, .color = { 0.0, 0.5, 1.0, 1.0 } },

        { .pos = { -1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = { -1.0,  1.0,  1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0,  1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } },
        { .pos = {  1.0,  1.0, -1.0, 1.0 }, .color = { 1.0, 0.0, 0.5, 1.0 } }
    };
    state.bind.vs.storage_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_STORAGEBUFFER,
        .data = SG_RANGE(vertices),
    });

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
        .data = SG_RANGE(indices)
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .vs.storage_buffers[0] = { .used = true, .readonly = true },
        .vs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vertex_t {\n"
            "  float4 pos;\n"
            "  float4 color;\n"
            "};\n"
            "struct ssbo_t {\n"
            "  vertex_t vtx[1];\n" // MSL C++ doesn't have flexible array member support
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float4 color;\n"
            "};\n"
            "vertex vs_out _main(constant params_t& params [[buffer(0)]], const device ssbo_t& ssbo [[buffer(12)]], uint vidx [[vertex_id]]) {\n"
            "  vs_out out;\n"
            "  out.pos = params.mvp * ssbo.vtx[vidx].pos;\n"
            "  out.color = ssbo.vtx[vidx].color;\n"
            "  return out;\n"
            "}\n",
        .fs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "}\n"
    });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });
}

static void frame(void) {
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)osx_width()/(float)osx_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    vs_params_t vs_params;
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = osx_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 4, SG_PIXELFORMAT_DEPTH, "vertexpulling-metal", init, frame, shutdown);
    return 0;
}
