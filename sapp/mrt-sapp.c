//------------------------------------------------------------------------------
//  mrt-sapp.c
//  Rendering with multi-rendertargets, and recreating render targets
//  when window size changes.
//------------------------------------------------------------------------------
#include <stddef.h> /* offsetof */
#include "sokol_app.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "mrt-sapp.glsl.h"

#define MSAA_SAMPLES (4)

static sg_pass_desc offscreen_pass_desc;
static sg_pass offscreen_pass;
static sg_pipeline offscreen_pip;
static sg_bindings offscreen_bind;
static sg_pipeline fsq_pip;
static sg_bindings fsq_bind;
static sg_pipeline dbg_pip;
static sg_bindings dbg_bind;

/* pass action to clear the MRT render target */
static const sg_pass_action offscreen_pass_action = {
    .colors = {
        [0] = { .action = SG_ACTION_CLEAR, .val = { 0.25f, 0.0f, 0.0f, 1.0f } },
        [1] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.25f, 0.0f, 1.0f } },
        [2] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.25f, 1.0f } }
    }
};

/* default pass action, no clear needed, since whole screen is overwritten */
static const sg_pass_action default_pass_action = {
    .colors = {
        [0].action = SG_ACTION_DONTCARE,
        [1].action = SG_ACTION_DONTCARE,
        [2].action = SG_ACTION_DONTCARE
    },
    .depth.action = SG_ACTION_DONTCARE,
    .stencil.action = SG_ACTION_DONTCARE
};

static float rx, ry;

typedef struct {
    float x, y, z, b;
} vertex_t;

/* called initially and when window size changes */
void create_offscreen_pass(int width, int height) {
    /* destroy previous resource (can be called for invalid id) */
    sg_destroy_pass(offscreen_pass);
    for (int i = 0; i < 3; i++) {
        sg_destroy_image(offscreen_pass_desc.color_attachments[i].image);
    }
    sg_destroy_image(offscreen_pass_desc.depth_stencil_attachment.image);

    /* create offscreen rendertarget images and pass */
    const int offscreen_sample_count = sg_query_features().msaa_render_targets ? MSAA_SAMPLES : 1;
    sg_image_desc color_img_desc = {
        .render_target = true,
        .width = width,
        .height = height,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = offscreen_sample_count,
        .label = "color image"
    };
    sg_image_desc depth_img_desc = color_img_desc;
    depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    depth_img_desc.label = "depth image";
    offscreen_pass_desc = (sg_pass_desc){
        .color_attachments = {
            [0].image = sg_make_image(&color_img_desc),
            [1].image = sg_make_image(&color_img_desc),
            [2].image = sg_make_image(&color_img_desc)
        },
        .depth_stencil_attachment.image = sg_make_image(&depth_img_desc),
        .label = "offscreen pass"
    };
    offscreen_pass = sg_make_pass(&offscreen_pass_desc);

    /* also need to update the fullscreen-quad texture bindings */
    for (int i = 0; i < 3; i++) {
        fsq_bind.fs_images[i] = offscreen_pass_desc.color_attachments[i].image;
    }
}

/* listen for window-resize events and recreate offscreen rendertargets */
void event(const sapp_event* e) {
    if (e->type == SAPP_EVENTTYPE_RESIZED) {
        create_offscreen_pass(e->framebuffer_width, e->framebuffer_height);
    }
    __dbgui_event(e);
}

void init(void) {
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
    __dbgui_setup(MSAA_SAMPLES);
    if (sapp_gles2()) {
        /* this demo needs GLES3/WebGL */
        return;
    }

    /* a render pass with 3 color attachment images, and a depth attachment image */
    create_offscreen_pass(sapp_width(), sapp_height());

    /* cube vertex buffer */
    vertex_t cube_vertices[] = {
        /* pos + brightness */
        { -1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f, -1.0f, -1.0f,   1.0f },
        {  1.0f,  1.0f, -1.0f,   1.0f },
        { -1.0f,  1.0f, -1.0f,   1.0f },

        { -1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f, -1.0f,  1.0f,   0.8f },
        {  1.0f,  1.0f,  1.0f,   0.8f },
        { -1.0f,  1.0f,  1.0f,   0.8f },

        { -1.0f, -1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f, -1.0f,   0.6f },
        { -1.0f,  1.0f,  1.0f,   0.6f },
        { -1.0f, -1.0f,  1.0f,   0.6f },

        { 1.0f, -1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f, -1.0f,    0.4f },
        { 1.0f,  1.0f,  1.0f,    0.4f },
        { 1.0f, -1.0f,  1.0f,    0.4f },

        { -1.0f, -1.0f, -1.0f,   0.5f },
        { -1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f,  1.0f,   0.5f },
        {  1.0f, -1.0f, -1.0f,   0.5f },

        { -1.0f,  1.0f, -1.0f,   0.7f },
        { -1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f,  1.0f,   0.7f },
        {  1.0f,  1.0f, -1.0f,   0.7f },
    };
    sg_buffer cube_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(cube_vertices),
        .content = cube_vertices,
        .label = "cube vertices"
    });

    /* index buffer for the cube */
    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer cube_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(cube_indices),
        .content = cube_indices,
        .label = "cube indices"
    });

    /* a shader to render the cube into offscreen MRT render targest */
    sg_shader offscreen_shd = sg_make_shader(offscreen_shader_desc());

    /* pipeline object for the offscreen-rendered cube */
    offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [ATTR_vs_offscreen_pos]     = { .offset=offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [ATTR_vs_offscreen_bright0] = { .offset=offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .blend = {
            .color_attachment_count = 3,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = MSAA_SAMPLES
        },
        .label = "offscreen pipeline"
    });

    /* resource bindings for offscreen rendering */
    offscreen_bind = (sg_bindings){
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    /* a vertex buffer to render a fullscreen rectangle */
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(quad_vertices),
        .content = quad_vertices,
        .label = "quad vertices"
    });

    /* a shader to render a fullscreen rectangle by adding the 3 offscreen-rendered images */
    sg_shader fsq_shd = sg_make_shader(fsq_shader_desc());

    /* the pipeline object to render the fullscreen quad */
    fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_vs_fsq_pos].format=SG_VERTEXFORMAT_FLOAT2
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .rasterizer.sample_count = MSAA_SAMPLES,
        .label = "fullscreen quad pipeline"
    });

    /* resource bindings to render a fullscreen quad */
    fsq_bind = (sg_bindings){
        .vertex_buffers[0] = quad_vbuf,
        .fs_images = {
            [SLOT_tex0] = offscreen_pass_desc.color_attachments[0].image,
            [SLOT_tex1] = offscreen_pass_desc.color_attachments[1].image,
            [SLOT_tex2] = offscreen_pass_desc.color_attachments[2].image
        }
    };

    /* pipeline and resource bindings to render debug-visualization quads */
    dbg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_vs_dbg_pos].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(dbg_shader_desc()),
        .rasterizer.sample_count = MSAA_SAMPLES,
        .label = "dbgvis quad pipeline"
    }),
    dbg_bind = (sg_bindings){
        .vertex_buffers[0] = quad_vbuf
        /* images will be filled right before rendering */
    };
}

/* this is called when GLES3/WebGL2 is not available */
void draw_gles2_fallback(void) {
    const sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } },
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void frame(void) {
    /* can't do anything useful on GLES2/WebGL */
    if (sapp_gles2()) {
        draw_gles2_fallback();
        return;
    }

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* shader parameters */
    offscreen_params_t offscreen_params;
    fsq_params_t fsq_params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    offscreen_params.mvp = HMM_MultiplyMat4(view_proj, model);
    fsq_params.offset = HMM_Vec2(HMM_SinF(rx*0.01f)*0.1f, HMM_SinF(ry*0.01f)*0.1f);

    /* render cube into MRT offscreen render targets */
    sg_begin_pass(offscreen_pass, &offscreen_pass_action);
    sg_apply_pipeline(offscreen_pip);
    sg_apply_bindings(&offscreen_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_offscreen_params, &offscreen_params, sizeof(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    /* render fullscreen quad with the 'composed image', plus 3
       small debug-view quads */
    sg_begin_default_pass(&default_pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(fsq_pip);
    sg_apply_bindings(&fsq_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_fsq_params, &fsq_params, sizeof(fsq_params));
    sg_draw(0, 4, 1);
    sg_apply_pipeline(dbg_pip);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        dbg_bind.fs_images[SLOT_tex] = offscreen_pass_desc.color_attachments[i].image;
        sg_apply_bindings(&dbg_bind);
        sg_draw(0, 4, 1);
    }
    sg_apply_viewport(0, 0, sapp_width(), sapp_height(), false);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 800,
        .height = 600,
        .sample_count = MSAA_SAMPLES,
        .window_title = "MRT Rendering (sokol-app)",
    };
}
