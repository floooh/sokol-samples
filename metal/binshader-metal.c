//------------------------------------------------------------------------------
//  binshader-metal.c
//  Use precompiled MSL shader byte code.
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#if !TARGET_OS_IPHONE
#include "binshader-metal-macosx.h"
#else
#include "binshader-metal-iphoneos.h"
#endif

const int WIDTH = 640;
const int HEIGHT = 480;
const int MSAA_SAMPLES = 4;

sg_pass_action pass_action = {0};
sg_draw_state draw_state = {0};
float rx = 0.0f;
float ry = 0.0f;
hmm_mat4 view_proj;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

void init(const void* mtl_device) {
    /* setup sokol */
    sg_desc desc = {
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    };
    sg_setup(&desc);

    /* cube vertex buffer */
    float vertices[] = {
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

        1.0, -1.0, -1.0,   1.0, 0.5, 0.0, 1.0,
        1.0,  1.0, -1.0,   1.0, 0.5, 0.0, 1.0,
        1.0,  1.0,  1.0,   1.0, 0.5, 0.0, 1.0,
        1.0, -1.0,  1.0,   1.0, 0.5, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

        -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
        -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
    };
    draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* create an index buffer for the cube */
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    draw_state.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices
    });

    /* shader as precompiled metal lib */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .byte_code = vs_bytecode,
            .byte_code_size = sizeof(vs_bytecode)
        },
        .fs = {
            .byte_code = fs_bytecode,
            .byte_code_size = sizeof(fs_bytecode)
        }
    });

    /* pipeline object for rendering the cube */
    draw_state.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = MSAA_SAMPLES
        }
    });

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);
}

void frame() {
    /* compute model-view-projection matrix for vertex shader */
    vs_params_t vs_params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, MSAA_SAMPLES, "Sokol Shader Bytecode (Metal)", init, frame, shutdown);
    return 0;
}
