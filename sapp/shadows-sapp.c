//------------------------------------------------------------------------------
//  shadows-sapp.c
//  Render to an offscreen rendertarget texture, and use this texture
//  for rendering shadows to the screen.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "shadows-sapp.glsl.h"

#define SCREEN_SAMPLE_COUNT (4)

static struct {
    struct {
        sg_pass_action pass_action;
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } shadows;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } deflt;
    float ry;
} state;

void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    /* default pass action: clear to blue-ish */
    state.deflt.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.25f, 1.0f, 1.0f } }
    };

    /* shadow pass action: clear to white */
    state.shadows.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 1.0f, 1.0f, 1.0f } }
    };

    /* a render pass with one color- and one depth-attachment image */
    sg_image_desc img_desc = {
        .render_target = true,
        .width = 2048,
        .height = 2048,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .sample_count = 1,
        .label = "shadow-map-color-image"
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    img_desc.label = "shadow-map-depth-image";
    sg_image depth_img = sg_make_image(&img_desc);
    state.shadows.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = color_img,
        .depth_stencil_attachment.image = depth_img,
        .label = "shadow-map-pass"
    });

    /* cube vertex buffer with positions & normals */
    float vertices[] = {
        /* pos                  normals             */
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
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
        .label = "cube-vertices"
    });

    /* an index buffer for the cube */
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20,
        26, 25, 24,  27, 26, 24
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
        .label = "cube-indices"
    });

    /* pipeline-state-object for shadows-rendered cube, don't need texture coord here */
    state.shadows.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* need to provide stride, because the buffer's normal vector is skipped */
            .buffers[0].stride = 6 * sizeof(float),
            /* but don't need to provide attr offsets, because pos and normal are continuous */
            .attrs = {
                [ATTR_shadowVS_position].format = SG_VERTEXFORMAT_FLOAT3
            }
        },
        .shader = sg_make_shader(shadow_shader_desc()),
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true,
        },
        .blend = {
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            /* Cull front faces in the shadow map pass */
            .cull_mode = SG_CULLMODE_FRONT,
            .sample_count = 1
        },
        .label = "shadow-map-pipeline"
    });

    /* and another pipeline-state-object for the default pass */
    state.deflt.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* don't need to provide buffer stride or attr offsets, no gaps here */
            .attrs = {
                [ATTR_colorVS_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_colorVS_normal].format = SG_VERTEXFORMAT_FLOAT3
            }
        },
        .shader = sg_make_shader(color_shader_desc()),
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer = {
            /* Cull back faces when rendering to the screen */
            .cull_mode = SG_CULLMODE_BACK,
        },
        .label = "default-pipeline"
    });

    /* the resource bindings for rendering the cube into the shadow map render target */
    state.shadows.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* resource bindings to render the cube, using the shadow map render target as texture */
    state.deflt.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[SLOT_shadowMap] = color_img
    };

    state.ry = 0.0f;
}

void frame(void) {

    state.ry += 0.2f;

    /* Calculate matrices for shadow pass */
    const hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f,1.0f,0.0f));
    const hmm_vec4 light_dir = HMM_MultiplyMat4ByVec4(rym, HMM_Vec4(50.0f,50.0f,-50.0f,0.0f));
    const hmm_mat4 light_view = HMM_LookAt(light_dir.XYZ, HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));

    /* Configure a bias matrix for converting view-space coordinates into uv coordinates */
    hmm_mat4 light_proj = { {
        { 0.5f, 0.0f, 0.0f, 0 },
        { 0.0f, 0.5f, 0.0f, 0 },
        { 0.0f, 0.0f, 0.5f, 0 },
        { 0.5f, 0.5f, 0.5f, 1 }
    } };
    light_proj = HMM_MultiplyMat4(light_proj, HMM_Orthographic(-4.0f, 4.0f, -4.0f, 4.0f, 0, 200.0f));
    const hmm_mat4 light_view_proj = HMM_MultiplyMat4(light_proj, light_view);

    /* Calculate matrices for camera pass */
    const hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 100.0f);
    const hmm_mat4 view = HMM_LookAt(HMM_Vec3(5.0f, 5.0f, 5.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* Calculate transform matrices for plane and cube */
    const hmm_mat4 scale = HMM_Scale(HMM_Vec3(5,0,5));
    const hmm_mat4 translate = HMM_Translate(HMM_Vec3(0,1.5f,0));

    /* Initialise fragment uniforms for light shader */
    const fs_light_params_t fs_light_params = {
        .lightDir = HMM_NormalizeVec3(light_dir.XYZ),
        .shadowMapSize = HMM_Vec2(2048,2048),
        .eyePos = HMM_Vec3(5.0f,5.0f,5.0f)
    };

    /* the shadow map pass, render the vertices into the depth image */
    sg_begin_pass(state.shadows.pass, &state.shadows.pass_action);
    sg_apply_pipeline(state.shadows.pip);
    sg_apply_bindings(&state.shadows.bind);
    {
        /* Render the cube into the shadow map */
        const vs_shadow_params_t vs_shadow_params = {
            .mvp = HMM_MultiplyMat4(light_view_proj,translate)
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_shadow_params, &vs_shadow_params, sizeof(vs_shadow_params));
        sg_draw(0, 36, 1);
    }
    sg_end_pass();

    /* and the display-pass, rendering the scene, using the previously rendered shadow map as a texture */
    sg_begin_default_pass(&state.deflt.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.deflt.pip);
    sg_apply_bindings(&state.deflt.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_light_params, &fs_light_params, sizeof(fs_light_params));
    {
        /* Render the plane in the light pass */
        const vs_light_params_t vs_light_params = {
            .mvp = HMM_MultiplyMat4(view_proj,scale),
            .lightMVP = HMM_MultiplyMat4(light_view_proj,scale),
            .model = HMM_Mat4d(1.0f),
            .diffColor = HMM_Vec3(0.5f,0.5f,0.5f)
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_light_params, &vs_light_params, sizeof(vs_light_params));
        sg_draw(36, 6, 1);
    }
    {
        /* Render the cube in the light pass */
        const vs_light_params_t vs_light_params = {
            .lightMVP = HMM_MultiplyMat4(light_view_proj,translate),
            .model = translate,
            .mvp = HMM_MultiplyMat4(view_proj,translate),
            .diffColor = HMM_Vec3(1.0f,1.0f,1.0f)
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_light_params, &vs_light_params, sizeof(vs_light_params));
        sg_draw(0, 36, 1);
    }

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
        .sample_count = SCREEN_SAMPLE_COUNT,
        .window_title = "Shadow Rendering (sokol-app)",
    };
}
