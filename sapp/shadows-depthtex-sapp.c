//------------------------------------------------------------------------------
//  shadows-depthtex-sapp.c
//
//  Shadow mapping via a depth-buffer texture.
//
//  - depth-only shadow pass (no color render targets)
//  - display pass samples shadow map with a comparison sampler
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "shadows-depthtex-sapp.glsl.h"

static struct {
    sg_image shadow_map;
    sg_sampler shadow_sampler;
    sg_buffer vbuf;
    sg_buffer ibuf;
    float ry;
    struct {
        sg_pass_action pass_action;
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } shadow;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } display;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // FIXME: replace with sokol-shape:
    // cube vertex buffer with positions & normals
    const float vertices[] = {
        // pos                  normals
        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f, -1.0f,  //CUBE BACK FACE
         1.0f, -1.0f, -1.0f,    0.0f, 0.0f, -1.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 0.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f, 1.0f,   //CUBE FRONT FACE
         1.0f, -1.0f,  1.0f,    0.0f, 0.0f, 1.0f,
         1.0f,  1.0f,  1.0f,    0.0f, 0.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    -1.0f, 0.0f, 0.0f,  //CUBE LEFT FACE
        -1.0f,  1.0f, -1.0f,    -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    -1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    -1.0f, 0.0f, 0.0f,

         1.0f, -1.0f, -1.0f,    1.0f, 0.0f, 0.0f,   //CUBE RIGHT FACE
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.0f, 0.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, -1.0f, 0.0f,  //CUBE BOTTOM FACE
        -1.0f, -1.0f,  1.0f,    0.0f, -1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.0f, -1.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    0.0f, -1.0f, 0.0f,

        -1.0f,  1.0f, -1.0f,    0.0f, 1.0f, 0.0f,   //CUBE TOP FACE
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 1.0f, 0.0f,

        -1.0f,  0.0f, -1.0f,    0.0f, 1.0f, 0.0f,   //PLANE GEOMETRY
        -1.0f,  0.0f,  1.0f,    0.0f, 1.0f, 0.0f,
         1.0f,  0.0f,  1.0f,    0.0f, 1.0f, 0.0f,
         1.0f,  0.0f, -1.0f,    0.0f, 1.0f, 0.0f,
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });

    // an index buffer for the cube
    const uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20,
        26, 25, 24,  27, 26, 24
    };
    state.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // shadow map pass action: only clear depth buffer, don't configure color and stencil actions,
    // because there are no color and stencil targets
    state.shadow.pass_action = (sg_pass_action){
        .depth = {
            .load_action = SG_LOADACTION_CLEAR,
            .store_action = SG_STOREACTION_STORE,
            .clear_value = 1.0f,
        },
    };

    // a shadow map render target which will serve as depth buffer
    state.shadow_map = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 1024,
        .height = 1024,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
        .label = "shadow-map",
    });

    // a comparison sampler which is used to sample the shadow map texture in the display pass
    state.shadow_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .compare = SG_COMPAREFUNC_GREATER,
        .label = "shadow-sampler",
    });

    // a pass object for the shadow pass
    state.shadow.pass = sg_make_pass(&(sg_pass_desc){
        .depth_stencil_attachment.image = state.shadow_map,
        .label = "shadow-pass",
    });

    // a pipeline object for the shadow pass
    state.shadow.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // need to provide vertex stride, because normal component is skipped in shadow pass
            .buffers[0].stride = 6 * sizeof(float),
            .attrs = {
                [ATTR_vs_shadow_pos].format = SG_VERTEXFORMAT_FLOAT3,
            },
        },
        .shader = sg_make_shader(shadow_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        // FIXME!
        .cull_mode = SG_CULLMODE_NONE,
        .sample_count = 1,
        // important: 'deactivate' the default color target
        .colors[0].pixel_format = SG_PIXELFORMAT_NONE,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        // NOTE: no color targets
        .label = "shadow-pipeline"
    });

    // a pipeline object for the display pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_vs_display_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_display_norm].format = SG_VERTEXFORMAT_FLOAT3,
            }
        },
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        // FIXME
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "display-pipeline",
    });

    // resource bindings to render shadow scene
    state.shadow.bind = (sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
    };

    // resource bindings to render display scene
    state.shadow.bind = (sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
        .fs = {
            .images[SLOT_shadow_map] = state.shadow_map,
            .samplers[SLOT_shadow_sampler] = state.shadow_sampler,
        },
    };
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.ry += 0.2f * t;

    // calculate transform matrices for plane and cube
    const hmm_mat4 scale = HMM_Scale(HMM_Vec3(5.0f, 0.0f, 5.0f));
    const hmm_mat4 translate = HMM_Translate(HMM_Vec3(0.0f, 1.5f, 0.0f));

    // calculate matrices for shadow pass
    const hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_vec4 light_dir = HMM_MultiplyMat4ByVec4(rym, HMM_Vec4(50.0f, 50.0f, -50.0f, 0.0f));
    const hmm_mat4 light_view = HMM_LookAt(light_dir.XYZ, HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));

    // configure a bias matrix for converting view-space coordinates into uv coordinates
    hmm_mat4 light_proj = { {
        { 0.5f, 0.0f, 0.0f, 0 },
        { 0.0f, 0.5f, 0.0f, 0 },
        { 0.0f, 0.0f, 0.5f, 0 },
        { 0.5f, 0.5f, 0.5f, 1 }
    } };
    light_proj = HMM_MultiplyMat4(light_proj, HMM_Orthographic(-4.0f, 4.0f, -4.0f, 4.0f, 0, 200.0f));
    const hmm_mat4 light_view_proj = HMM_MultiplyMat4(light_proj, light_view);

    const vs_shadow_params_t vs_shadow_params = {
        .mvp = HMM_MultiplyMat4(light_view_proj, translate)
    };

    // calculate matrices for display pass
    const hmm_mat4 proj = HMM_Perspective(60.0f, sapp_widthf()/sapp_heightf(), 0.01f, 100.0f);
    const hmm_mat4 view = HMM_LookAt(HMM_Vec3(5.0f, 5.0f, 5.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);


    // the shadow map pass, render scene into shadow map texture
    sg_begin_pass(state.shadow.pass, &state.shadow.pass_action);
    sg_apply_pipeline(state.shadow.pip);
    sg_apply_bindings(&state.shadow.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_shadow_params, &SG_RANGE(vs_shadow_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // FIXME
    sg_begin_default_pass(&state.display.pass_action, sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
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
        .window_title = "shadows-depthtex-sapp",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
