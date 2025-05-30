//------------------------------------------------------------------------------
//  noentry-sapp.c
//
//  This demonstrates the SOKOL_NO_ENTRY mode of sokol_app.h, in this mode
//  sokol_app.h doesn't "hijack" the platform's main() function instead the
//  application must provide this. The sokol_app.h implementation must be
//  compiled with the SOKOL_NO_ENTRY define (see sokol-noentry.c/.m,
//  which is compiled into a static link lib sokol-noentry)
//
//  This sample also demonstrates the optional user-data callbacks.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_DLL
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include <stdlib.h>     // calloc, free
#include "noentry-dll-sapp.glsl.h"
#if defined(_WIN32)
#include <Windows.h>
#endif

typedef struct {
    float rx, ry;
    sg_pipeline pip;
    sg_bindings bind;
} app_state_t;

// user-provided callback prototypes
void init(void* user_data);
void frame(void* user_data);
void cleanup(void);

// don't provide a sokol_main() callback, instead the platform's standard main() function
#if defined(_WIN32)
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
#else
int main() {
#endif
    app_state_t* state = calloc(1, sizeof(app_state_t));
    sapp_run(&(sapp_desc){
        .user_data = state,
        .init_userdata_cb = init,
        .frame_userdata_cb = frame,
        .cleanup_cb = cleanup, // cleanup doesn't need access to the state struct
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "Noentry DLL (sokol-app)",
        .logger.func = slog_func,
    });
    free(state);    // NOTE: on some platforms, this isn't reached on exit
    return 0;
}

void init(void* user_data) {
    app_state_t* state = (app_state_t*) user_data;
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    // cube vertex buffer
    float vertices[] = {
        -1.0, -1.0, -1.0,   1.0, 0.5, 0.0, 1.0,
         1.0, -1.0, -1.0,   1.0, 0.5, 0.0, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.5, 0.0, 1.0,
        -1.0,  1.0, -1.0,   1.0, 0.5, 0.0, 1.0,

        -1.0, -1.0,  1.0,   0.5, 1.0, 0.0, 1.0,
         1.0, -1.0,  1.0,   0.5, 1.0, 0.0, 1.0,
         1.0,  1.0,  1.0,   0.5, 1.0, 0.0, 1.0,
        -1.0,  1.0,  1.0,   0.5, 1.0, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0,  1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,

        1.0, -1.0, -1.0,    1.0, 0.5, 0.5, 1.0,
        1.0,  1.0, -1.0,    1.0, 0.5, 0.5, 1.0,
        1.0,  1.0,  1.0,    1.0, 0.5, 0.5, 1.0,
        1.0, -1.0,  1.0,    1.0, 0.5, 0.5, 1.0,

        -1.0, -1.0, -1.0,   0.5, 0.5, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.5, 0.5, 1.0, 1.0,
         1.0, -1.0,  1.0,   0.5, 0.5, 1.0, 1.0,
         1.0, -1.0, -1.0,   0.5, 0.5, 1.0, 1.0,

        -1.0,  1.0, -1.0,   0.5, 1.0, 0.5, 1.0,
        -1.0,  1.0,  1.0,   0.5, 1.0, 0.5, 1.0,
         1.0,  1.0,  1.0,   0.5, 1.0, 0.5, 1.0,
         1.0,  1.0, -1.0,   0.5, 1.0, 0.5, 1.0
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
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
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // create shader
    sg_shader shd = sg_make_shader(noentry_shader_desc(sg_query_backend()));

    // create pipeline object
    state->pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // test to provide buffer stride, but no attr offsets
            .buffers[0].stride = 28,
            .attrs = {
                [ATTR_noentry_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_noentry_color0].format = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    // setup resource bindings
    state->bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };
}

void frame(void* user_data) {
    app_state_t* state = (app_state_t*) user_data;
    vs_params_t vs_params;
    const float w = (float) sapp_width();
    const float h = (float) sapp_height();
    const float t = (float)(sapp_frame_duration() * 60.0);
    hmm_mat4 proj = HMM_Perspective(60.0f, w/h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state->rx += 1.0f * t; state->ry += 2.0f * t;
    hmm_mat4 rxm = HMM_Rotate(state->rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state->ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.5f, 0.25f, 0.75f, 1.0f } }
        },
        .swapchain = sglue_swapchain()
    });
    sg_apply_pipeline(state->pip);
    sg_apply_bindings(&state->bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

void cleanup() {
    sg_shutdown();
}
