//------------------------------------------------------------------------------
//  shadows-sapp.c
//
//  Shadow mapping via a regular RGBA8 texture as shadow map. The depth value
//  is encoded to RGBA8 in the shadow pass fragment shader, and decoded
//  from RGBA8 in the display-pass fragment shader (this encoding was required for
//  WebGL compatibility across all devices, but is probably no longer needed
//  in WebGL2 and could be replaced with a regular R32F texture instead.
//
//  Also see shadows-depthtex-sapp for a similar sample using a depth-only
//  pass and depth-buffer as shadow map.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "shadows-sapp.glsl.h"

static struct {
    sg_image shadow_map;
    sg_sampler shadow_sampler;
    sg_buffer vbuf;
    sg_buffer ibuf;
    float ry;
    struct {
        sg_pass_action pass_action;
        sg_attachments atts;
        sg_pipeline pip;
        sg_bindings bind;
    } shadow;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } display;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
    } dbg;
} state;

void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // vertex buffer for a cube and plane
    const float scene_vertices[] = {
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

        -5.0f,  0.0f, -5.0f,    0.0f, 1.0f, 0.0f,   //PLANE GEOMETRY
        -5.0f,  0.0f,  5.0f,    0.0f, 1.0f, 0.0f,
         5.0f,  0.0f,  5.0f,    0.0f, 1.0f, 0.0f,
         5.0f,  0.0f, -5.0f,    0.0f, 1.0f, 0.0f,
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(scene_vertices),
        .label = "cube-vertices"
    });

    // ...and a matching index buffer for the scene
    const uint16_t scene_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20,
        26, 25, 24,  27, 26, 24
    };
    state.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(scene_indices),
        .label = "cube-indices"
    });

    // shadow map pass action: clear the shadow map to (1,1,1,1)
    state.shadow.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 1.0f, 1.0f, 1.0f, 1.0f },
        }
    };

    // display pass action
    state.display.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.5f, 0.25f, 1.0f}
        },
    };

    // a regular RGBA8 render target image as shadow map
    state.shadow_map = sg_make_image(&(sg_image_desc){
        .usage.render_attachment = true,
        .width = 2048,
        .height = 2048,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "shadow-map",
    });

    // ...we also need a separate depth-buffer image for the shadow pass
    sg_image shadow_depth_img = sg_make_image(&(sg_image_desc){
        .usage.render_attachment = true,
        .width = 2048,
        .height = 2048,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
        .label = "shadow-depth-buffer",
    });

    // a regular sampler with nearest filtering to sample the shadow map
    state.shadow_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "shadow-sampler",
    });

    // the render pass object for the shadow pass
    state.shadow.atts = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = state.shadow_map,
        .depth_stencil.image = shadow_depth_img,
        .label = "shadow-pass",
    });

    // a pipeline object for the shadow pass
    state.shadow.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            // need to provide vertex stride, because normal component is skipped in shadow pass
            .buffers[0].stride = 6 * sizeof(float),
            .attrs = {
                [ATTR_shadow_pos].format = SG_VERTEXFORMAT_FLOAT3,
            },
        },
        .shader = sg_make_shader(shadow_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        // render back-faces in shadow pass to prevent shadow acne on front-faces
        .cull_mode = SG_CULLMODE_FRONT,
        .sample_count = 1,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "shadow-pipeline"
    });

    // resource bindings to render shadow scene
    state.shadow.bind = (sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
    };

    // a pipeline object for the display pass
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_display_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_display_norm].format = SG_VERTEXFORMAT_FLOAT3,
            }
        },
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "display-pipeline",
    });

    // resource bindings to render display scene
    state.display.bind = (sg_bindings) {
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
        .images[IMG_shadow_map] = state.shadow_map,
        .samplers[SMP_shadow_sampler] = state.shadow_sampler,
    };

    // a vertex buffer, pipeline and sampler to render a debug visualization of the shadow map
    float dbg_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer dbg_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(dbg_vertices),
        .label = "debug-vertices"
    });
    state.dbg.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_dbg_pos].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .shader = sg_make_shader(dbg_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "debug-pipeline",
    });
    sg_sampler dbg_smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "debug-sampler"
    });
    state.dbg.bind = (sg_bindings){
        .vertex_buffers[0] = dbg_vbuf,
        .images[IMG_dbg_tex] = state.shadow_map,
        .samplers[SMP_dbg_smp] = dbg_smp,
    };
}

void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.ry += 0.2f * t;

    const hmm_vec3 eye_pos = HMM_Vec3(5.0f, 5.0f, 5.0f);
    const hmm_mat4 plane_model = HMM_Mat4d(1.0f);
    const hmm_mat4 cube_model = HMM_Translate(HMM_Vec3(0.0f, 1.5f, 0.0f));
    const hmm_vec3 plane_color = HMM_Vec3(1.0f, 0.5f, 0.0f);
    const hmm_vec3 cube_color = HMM_Vec3(0.5f, 0.5f, 1.0f);

    // calculate matrices for shadow pass
    const hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_vec4 light_pos = HMM_MultiplyMat4ByVec4(rym, HMM_Vec4(50.0f, 50.0f, -50.0f, 1.0f));
    const hmm_mat4 light_view = HMM_LookAt(light_pos.XYZ, HMM_Vec3(0.0f, 1.5f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_mat4 light_proj = HMM_Orthographic(-5.0f, 5.0f, -5.0f, 5.0f, 0, 100.0f);
    const hmm_mat4 light_view_proj = HMM_MultiplyMat4(light_proj, light_view);

    const vs_shadow_params_t cube_vs_shadow_params = {
        .mvp = HMM_MultiplyMat4(light_view_proj, cube_model)
    };

    // calculate matrices for display pass
    const hmm_mat4 proj = HMM_Perspective(60.0f, sapp_widthf()/sapp_heightf(), 0.01f, 100.0f);
    const hmm_mat4 view = HMM_LookAt(eye_pos, HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    const fs_display_params_t fs_display_params = {
        .light_dir = HMM_NormalizeVec3(light_pos.XYZ),
        .eye_pos = eye_pos,
    };

    const vs_display_params_t plane_vs_display_params = {
        .mvp = HMM_MultiplyMat4(view_proj, plane_model),
        .model = plane_model,
        .light_mvp = HMM_MultiplyMat4(light_view_proj, plane_model),
        .diff_color = plane_color,
    };

    const vs_display_params_t cube_vs_display_params = {
        .mvp = HMM_MultiplyMat4(view_proj, cube_model),
        .model = cube_model,
        .light_mvp = HMM_MultiplyMat4(light_view_proj, cube_model),
        .diff_color = cube_color,
    };

    // the shadow map pass, render scene from light source into shadow map texture
    sg_begin_pass(&(sg_pass){ .action = state.shadow.pass_action, .attachments = state.shadow.atts });
    sg_apply_pipeline(state.shadow.pip);
    sg_apply_bindings(&state.shadow.bind);
    sg_apply_uniforms(UB_vs_shadow_params, &SG_RANGE(cube_vs_shadow_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // the display pass, render scene from camera and sample the shadow map
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(UB_fs_display_params, &SG_RANGE(fs_display_params));
    // render plane
    sg_apply_uniforms(UB_vs_display_params, &SG_RANGE(plane_vs_display_params));
    sg_draw(36, 6, 1);
    // render cube
    sg_apply_uniforms(UB_vs_display_params, &SG_RANGE(cube_vs_display_params));
    sg_draw(0, 36, 1);
    // render debug visualization of shadow-map
    sg_apply_pipeline(state.dbg.pip);
    sg_apply_bindings(&state.dbg.bind);
    sg_apply_viewport(sapp_width() - 150, 0, 150, 150, false);
    sg_draw(0, 4, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
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
        .window_title = "shadows-sapp",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
