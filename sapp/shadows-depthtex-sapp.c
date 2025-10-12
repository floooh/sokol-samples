//------------------------------------------------------------------------------
//  shadows-depthtex-sapp.c
//
//  Shadow mapping via a depth-buffer texture.
//
//  - depth-only shadow pass into shadow map (no color render targets)
//  - display pass samples shadow map with a comparison sampler
//  - debug visualization samples shadow map with a regular sampler
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "shadows-depthtex-sapp.glsl.h"

static struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    float ry;
    struct {
        sg_pass pass;
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

static void init(void) {
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

    // display pass action
    state.display.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.25f, 0.5f, 1.0f}
        },
    };

    // a shadow map render target which will serve as depth buffer in the shadow pass
    sg_image shadow_map_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = 2048,
        .height = 2048,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
        .label = "shadow-map",
    });

    // a pass attachment and a texture view for the shadow map render target
    sg_view shadow_map_ds_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment = { .image = shadow_map_img },
        .label = "shadow-map-depth-stencil-view",
    });
    sg_view shadow_map_tex_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = shadow_map_img },
        .label = "shadow-map-tex-view",
    });

    // shadow render pass descriptor
    state.shadow.pass = (sg_pass) {
        // shadow map pass action: only clear depth buffer, don't configure color and stencil actions,
        // because there are no color and stencil targets
        .action.depth = {
            .load_action = SG_LOADACTION_CLEAR,
            .store_action = SG_STOREACTION_STORE,
            .clear_value = 1.0f,
        },
        // the pass only has a depth-stencil attachment
        .attachments.depth_stencil = shadow_map_ds_view,
    };


    // a comparison sampler which is used to sample the shadow map texture in the display pass
    sg_sampler shadow_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .compare = SG_COMPAREFUNC_LESS,
        .label = "shadow-sampler",
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
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        // important: 'deactivate' the default color target for 'depth-only-rendering'
        .colors[0].pixel_format = SG_PIXELFORMAT_NONE,
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
        .views[VIEW_shadow_map] = shadow_map_tex_view,
        .samplers[SMP_shadow_sampler] = shadow_sampler,
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
    // note: use a regular sampling-sampler to render the shadow map,
    // need to use nearest filtering here because of portability restrictions
    // (e.g. WebGL2)
    sg_sampler dbg_smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "debug-sampler"
    });
    state.dbg.bind = (sg_bindings){
        .vertex_buffers[0] = dbg_vbuf,
        .views[VIEW_dbg_tex] = shadow_map_tex_view,
        .samplers[SMP_dbg_smp] = dbg_smp,
    };
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.ry += 0.2f * t;

    const vec3_t eye_pos = vec3(5.0f, 5.0f, 5.0f);
    const mat44_t plane_model = mat44_identity();
    const mat44_t cube_model = mat44_translation(0.0f, 1.5f, 0.0f);
    const vec3_t plane_color = vec3(1.0f, 0.5f, 0.0f);
    const vec3_t cube_color = vec3(0.5f, 1.0f, 0.5f);

    // calculate matrices for shadow pass
    const mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    const vec4_t light_pos = vec4_transform(vec4(50.0f, 50.0f, -50.0f, 1.0f), rym);
    const mat44_t light_view = mat44_look_at_rh(vec4_xyz(light_pos), vec3(0.0f, 1.5f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t light_proj = mat44_ortho_off_center_rh(-5.0f, 5.0f, -5.0f, 5.0f, 0, 100.0f);
    const mat44_t light_view_proj = vm_mul(light_view, light_proj);

    const vs_shadow_params_t cube_vs_shadow_params = {
        .mvp = vm_mul(cube_model, light_view_proj)
    };

    // calculate matrices for display pass
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), sapp_widthf()/sapp_heightf(), 0.01f, 100.0f);
    const mat44_t view = mat44_look_at_rh(eye_pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t view_proj = vm_mul(view, proj);

    const fs_display_params_t fs_display_params = {
        .light_dir = vm_normalize(vec4_xyz(light_pos)),
        .eye_pos = eye_pos,
    };

    const vs_display_params_t plane_vs_display_params = {
        .mvp = vm_mul(plane_model, view_proj),
        .model = plane_model,
        .light_mvp = vm_mul(plane_model, light_view_proj),
        .diff_color = plane_color,
    };

    const vs_display_params_t cube_vs_display_params = {
        .mvp = vm_mul(cube_model, view_proj),
        .model = cube_model,
        .light_mvp = vm_mul(cube_model, light_view_proj),
        .diff_color = cube_color,
    };

    // the shadow map pass, render scene from light source into shadow map texture
    sg_begin_pass(&state.shadow.pass);
    sg_apply_pipeline(state.shadow.pip);
    sg_apply_bindings(&state.shadow.bind);
    sg_apply_uniforms(UB_vs_shadow_params, &SG_RANGE(cube_vs_shadow_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // the display pass, render scene from camera, compare-sample shadow map texture
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain_next() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(UB_fs_display_params, &SG_RANGE(fs_display_params));
    // render plane
    sg_apply_uniforms(UB_vs_display_params , &SG_RANGE(plane_vs_display_params));
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
