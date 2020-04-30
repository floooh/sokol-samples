//------------------------------------------------------------------------------
//  mrt-wgpu.c
//  Rendering with multi-rendertargets, and recreating render targets
//  when window size changes.
//------------------------------------------------------------------------------
#include <stddef.h> /* offsetof */
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "wgpu_entry.h"
#include "mrt-wgpu.glsl.h"

#define OFFSCREEN_DEPTH_FORMAT SG_PIXELFORMAT_DEPTH_STENCIL
#define OFFSCREEN_MSAA_SAMPLES (4)
#define DISPLAY_MSAA_SAMPLES (1)

static struct {
    struct {
        sg_pass_action pass_action;
        sg_pass_desc pass_desc;
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
    } fsq;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
    } dbg;
    sg_pass_action pass_action;
    int width, height;
    float rx, ry;
} state;

typedef struct {
    float x, y, z, b;
} vertex_t;

/* called initially and when window size changes */
static void create_offscreen_pass(int width, int height) {
    /* destroy previous resource (can be called for invalid id) */
    sg_destroy_pass(state.offscreen.pass);
    for (int i = 0; i < 3; i++) {
        sg_destroy_image(state.offscreen.pass_desc.color_attachments[i].image);
    }
    sg_destroy_image(state.offscreen.pass_desc.depth_stencil_attachment.image);

    /* create offscreen rendertarget images and pass */
    sg_image_desc color_img_desc = {
        .render_target = true,
        .width = width,
        .height = height,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = OFFSCREEN_MSAA_SAMPLES,
        .label = "color image"
    };
    sg_image_desc depth_img_desc = color_img_desc;
    depth_img_desc.pixel_format = OFFSCREEN_DEPTH_FORMAT;
    depth_img_desc.label = "depth image";
    state.offscreen.pass_desc = (sg_pass_desc){
        .color_attachments = {
            [0].image = sg_make_image(&color_img_desc),
            [1].image = sg_make_image(&color_img_desc),
            [2].image = sg_make_image(&color_img_desc)
        },
        .depth_stencil_attachment.image = sg_make_image(&depth_img_desc),
        .label = "offscreen pass"
    };
    state.offscreen.pass = sg_make_pass(&state.offscreen.pass_desc);

    /* also need to update the fullscreen-quad texture bindings */
    for (int i = 0; i < 3; i++) {
        state.fsq.bind.fs_images[i] = state.offscreen.pass_desc.color_attachments[i].image;
    }
}

static void init(void) {
    sg_setup(&(sg_desc){
        .context = wgpu_get_context()
    });

    /* a pass action for the default render pass */
    state.pass_action = (sg_pass_action) {
        .colors = {
            [0].action = SG_ACTION_DONTCARE,
            [1].action = SG_ACTION_DONTCARE,
            [2].action = SG_ACTION_DONTCARE
        },
        .depth.action = SG_ACTION_DONTCARE,
        .stencil.action = SG_ACTION_DONTCARE
    };

    /* a render pass with 3 color attachment images, and a depth attachment image */
    state.width = wgpu_width();
    state.height = wgpu_height();
    create_offscreen_pass(state.width, state.height);

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

    /* pass action for offscreen pass */
    state.offscreen.pass_action = (sg_pass_action) {
        .colors = {
            [0] = { .action = SG_ACTION_CLEAR, .val = { 0.25f, 0.0f, 0.0f, 1.0f } },
            [1] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.25f, 0.0f, 1.0f } },
            [2] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.25f, 1.0f } }
        }
    };

    /* pipeline object for the offscreen-rendered cube */
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
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
            .depth_format = OFFSCREEN_DEPTH_FORMAT,
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = OFFSCREEN_MSAA_SAMPLES
        },
        .label = "offscreen pipeline"
    });

    /* resource bindings for offscreen rendering */
    state.offscreen.bind = (sg_bindings){
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
    state.fsq.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_vs_fsq_pos].format=SG_VERTEXFORMAT_FLOAT2
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "fullscreen quad pipeline"
    });

    /* resource bindings to render a fullscreen quad */
    state.fsq.bind = (sg_bindings){
        .vertex_buffers[0] = quad_vbuf,
        .fs_images = {
            [SLOT_tex0] = state.offscreen.pass_desc.color_attachments[0].image,
            [SLOT_tex1] = state.offscreen.pass_desc.color_attachments[1].image,
            [SLOT_tex2] = state.offscreen.pass_desc.color_attachments[2].image
        }
    };

    /* pipeline and resource bindings to render debug-visualization quads */
    state.dbg.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[ATTR_vs_dbg_pos].format=SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(dbg_shader_desc()),
        .label = "dbgvis quad pipeline"
    }),
    state.dbg.bind = (sg_bindings){
        .vertex_buffers[0] = quad_vbuf
        /* images will be filled right before rendering */
    };
}

/* this is called when GLES3/WebGL2 is not available */
static void frame(void) {
    /* default framebuffer size changed? */
    const int w = wgpu_width();
    const int h = wgpu_height();
    if ((w != state.width) || (h != state.height)) {
        state.width = w;
        state.height = h;
        create_offscreen_pass(w, h);
    }

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)h/(float)w, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* shader parameters */
    offscreen_params_t offscreen_params;
    fsq_params_t fsq_params;
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    offscreen_params.mvp = HMM_MultiplyMat4(view_proj, model);
    fsq_params.offset = HMM_Vec2(HMM_SinF(state.rx*0.01f)*0.1f, HMM_SinF(state.ry*0.01f)*0.1f);

    /* render cube into MRT offscreen render targets */
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_offscreen_params, &offscreen_params, sizeof(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    /* render fullscreen quad with the 'composed image', plus 3 small debug-view quads */
    sg_begin_default_pass(&state.pass_action, w, h);
    sg_apply_pipeline(state.fsq.pip);
    sg_apply_bindings(&state.fsq.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_fsq_params, &fsq_params, sizeof(fsq_params));
    sg_draw(0, 4, 1);
    sg_apply_pipeline(state.dbg.pip);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        state.dbg.bind.fs_images[SLOT_tex] = state.offscreen.pass_desc.color_attachments[i].image;
        sg_apply_bindings(&state.dbg.bind);
        sg_draw(0, 4, 1);
    }
    sg_apply_viewport(0, 0, w, h, false);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .width = 640,
        .height = 480,
        .sample_count = DISPLAY_MSAA_SAMPLES,
        .title = "mrt-wgpu"
    });
    return 0;
}
