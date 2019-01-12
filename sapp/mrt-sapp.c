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

#define MSAA_SAMPLES (4)

static const char *cube_vs, *cube_fs, *fsq_vs, *fsq_fs, *dbg_vs, *dbg_fs;

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

typedef struct {
    hmm_mat4 mvp;
} offscreen_params_t;

typedef struct {
    hmm_vec2 offset;
} params_t;

/* called initially and when window size changes */
void create_offscreen_pass(int width, int height) {
    /* destroy previous resource (can be called for invalid id) */
    sg_destroy_pass(offscreen_pass);
    for (int i = 0; i < 3; i++) {
        sg_destroy_image(offscreen_pass_desc.color_attachments[i].image);
    }
    sg_destroy_image(offscreen_pass_desc.depth_stencil_attachment.image);

    /* create offscreen rendertarget images and pass */
    const int offscreen_sample_count = sg_query_feature(SG_FEATURE_MSAA_RENDER_TARGETS) ? MSAA_SAMPLES : 1;
    sg_image_desc color_img_desc = {
        .render_target = true,
        .width = width,
        .height = height,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = offscreen_sample_count
    };
    sg_image_desc depth_img_desc = color_img_desc;
    depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    offscreen_pass_desc = (sg_pass_desc){
        .color_attachments = {
            [0].image = sg_make_image(&color_img_desc),
            [1].image = sg_make_image(&color_img_desc),
            [2].image = sg_make_image(&color_img_desc)
        },
        .depth_stencil_attachment.image = sg_make_image(&depth_img_desc)
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
    });

    /* a shader to render the cube into offscreen MRT render targest */
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(offscreen_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .vs.source = cube_vs,
        .fs.source = cube_fs,
    });

    /* pipeline object for the offscreen-rendered cube */
    offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [0] = { .name="position", .sem_name="POSITION", .offset=offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="bright0", .sem_name="BRIGHT", .offset=offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
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
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = MSAA_SAMPLES
        }
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
        .content = quad_vertices
    });

    /* a shader to render a fullscreen rectangle by adding the 3 offscreen-rendered images */
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(params_t),
            .uniforms = {
                [0] = { .name="offset", .type=SG_UNIFORMTYPE_FLOAT2}
            }
        },
        .fs.images = {
            [0] = { .name="tex0", .type=SG_IMAGETYPE_2D },
            [1] = { .name="tex1", .type=SG_IMAGETYPE_2D },
            [2] = { .name="tex2", .type=SG_IMAGETYPE_2D }
        },
        .vs.source = fsq_vs,
        .fs.source = fsq_fs
    });

    /* the pipeline object to render the fullscreen quad */
    fsq_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0] = { .name="pos", .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT2 }
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .rasterizer.sample_count = MSAA_SAMPLES
    });

    /* resource bindings to render a fullscreen quad */
    fsq_bind = (sg_bindings){
        .vertex_buffers[0] = quad_vbuf,
        .fs_images = {
            [0] = offscreen_pass_desc.color_attachments[0].image,
            [1] = offscreen_pass_desc.color_attachments[1].image,
            [2] = offscreen_pass_desc.color_attachments[2].image
        }
    };

    /* pipeline and resource bindings to render debug-visualization quads */
    dbg_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0] = { .name="pos", .sem_name="POSITION", .format=SG_VERTEXFORMAT_FLOAT2 }
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(&(sg_shader_desc){
            .vs.source = dbg_vs,
            .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
            .fs.source = dbg_fs
        }),
        .rasterizer.sample_count = MSAA_SAMPLES
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
    params_t params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    offscreen_params.mvp = HMM_MultiplyMat4(view_proj, model);
    params.offset = HMM_Vec2(HMM_SinF(rx*0.01f)*0.1f, HMM_SinF(ry*0.01f)*0.1f);

    /* render cube into MRT offscreen render targets */
    sg_begin_pass(offscreen_pass, &offscreen_pass_action);
    sg_apply_pipeline(offscreen_pip);
    sg_apply_bindings(&offscreen_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &offscreen_params, sizeof(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    /* render fullscreen quad with the 'composed image', plus 3
       small debug-view quads */
    sg_begin_default_pass(&default_pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(fsq_pip);
    sg_apply_bindings(&fsq_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &params, sizeof(params));
    sg_draw(0, 4, 1);
    sg_apply_pipeline(dbg_pip);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        dbg_bind.fs_images[0] = offscreen_pass_desc.color_attachments[i].image;
        sg_apply_bindings(&dbg_bind);
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
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

#if defined(SOKOL_GLCORE33)
static const char* cube_vs =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec4 position;\n"
    "in float bright0;\n"
    "out float bright;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  bright = bright0;\n"
    "}\n";
static const char* cube_fs =
    "#version 330\n"
    "in float bright;\n"
    "layout(location=0) out vec4 frag_color_0;\n"
    "layout(location=1) out vec4 frag_color_1;\n"
    "layout(location=2) out vec4 frag_color_2;\n"
    "void main() {\n"
    "  frag_color_0 = vec4(bright, 0.0, 0.0, 1.0);\n"
    "  frag_color_1 = vec4(0.0, bright, 0.0, 1.0);\n"
    "  frag_color_2 = vec4(0.0, 0.0, bright, 1.0);\n"
    "}\n";
static const char* fsq_vs =
    "#version 330\n"
    "uniform vec2 offset;"
    "in vec2 pos;\n"
    "out vec2 uv0;\n"
    "out vec2 uv1;\n"
    "out vec2 uv2;\n"
    "void main() {\n"
    "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
    "  uv0 = pos + vec2(offset.x, 0.0);\n"
    "  uv1 = pos + vec2(0.0, offset.y);\n"
    "  uv2 = pos;\n"
    "}\n";
static const char* fsq_fs =
    "#version 330\n"
    "uniform sampler2D tex0;\n"
    "uniform sampler2D tex1;\n"
    "uniform sampler2D tex2;\n"
    "in vec2 uv0;\n"
    "in vec2 uv1;\n"
    "in vec2 uv2;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  vec3 c0 = texture(tex0, uv0).xyz;\n"
    "  vec3 c1 = texture(tex1, uv1).xyz;\n"
    "  vec3 c2 = texture(tex2, uv2).xyz;\n"
    "  frag_color = vec4(c0 + c1 + c2, 1.0);\n"
    "}\n";
static const char* dbg_vs =
    "#version 330\n"
    "in vec2 pos;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
    "  uv = pos;\n"
    "}\n";
static const char* dbg_fs =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = vec4(texture(tex,uv).xyz, 1.0);\n"
    "}\n";
#elif defined(SOKOL_GLES3)
static const char* cube_vs =
    "#version 300 es\n"
    "uniform mat4 mvp;\n"
    "in vec4 position;\n"
    "in float bright0;\n"
    "out float bright;\n"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  bright = bright0;\n"
    "}\n";
static const char* cube_fs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in float bright;\n"
    "layout(location=0) out vec4 frag_color_0;\n"
    "layout(location=1) out vec4 frag_color_1;\n"
    "layout(location=2) out vec4 frag_color_2;\n"
    "void main() {\n"
    "  frag_color_0 = vec4(bright, 0.0, 0.0, 1.0);\n"
    "  frag_color_1 = vec4(0.0, bright, 0.0, 1.0);\n"
    "  frag_color_2 = vec4(0.0, 0.0, bright, 1.0);\n"
    "}\n";
static const char* fsq_vs =
    "#version 300 es\n"
    "uniform vec2 offset;"
    "in vec2 pos;\n"
    "out vec2 uv0;\n"
    "out vec2 uv1;\n"
    "out vec2 uv2;\n"
    "void main() {\n"
    "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
    "  uv0 = pos + vec2(offset.x, 0.0);\n"
    "  uv1 = pos + vec2(0.0, offset.y);\n"
    "  uv2 = pos;\n"
    "}\n";
static const char* fsq_fs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D tex0;\n"
    "uniform sampler2D tex1;\n"
    "uniform sampler2D tex2;\n"
    "in vec2 uv0;\n"
    "in vec2 uv1;\n"
    "in vec2 uv2;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  vec3 c0 = texture(tex0, uv0).xyz;\n"
    "  vec3 c1 = texture(tex1, uv1).xyz;\n"
    "  vec3 c2 = texture(tex2, uv2).xyz;\n"
    "  frag_color = vec4(c0 + c1 + c2, 1.0);\n"
    "}\n";
static const char* dbg_vs =
    "#version 300 es\n"
    "uniform vec2 offset;"
    "in vec2 pos;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);\n"
    "  uv = pos;\n"
    "}\n";
static const char* dbg_fs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = vec4(texture(tex,uv).xyz, 1.0);\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char* cube_vs =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 pos [[attribute(0)]];\n"
    "  float bright [[attribute(1)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float bright;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.pos;\n"
    "  out.bright = in.bright;\n"
    "  return out;\n"
    "}\n";
static const char* cube_fs =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_out {\n"
    "  float4 color0 [[color(0)]];\n"
    "  float4 color1 [[color(1)]];\n"
    "  float4 color2 [[color(2)]];\n"
    "};\n"
    "fragment fs_out _main(float bright [[stage_in]]) {\n"
    "  fs_out out;\n"
    "  out.color0 = float4(bright, 0.0, 0.0, 1.0);\n"
    "  out.color1 = float4(0.0, bright, 0.0, 1.0);\n"
    "  out.color2 = float4(0.0, 0.0, bright, 1.0);\n"
    "  return out;\n"
    "}\n";
static const char* fsq_vs =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float2 offset;\n"
    "};\n"
    "struct vs_in {\n"
    "  float2 pos [[attribute(0)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float2 uv0;\n"
    "  float2 uv1;\n"
    "  float2 uv2;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = float4(in.pos*2.0-1.0, 0.5, 1.0);\n"
    "  out.uv0 = in.pos + float2(params.offset.x, 0.0);\n"
    "  out.uv1 = in.pos + float2(0.0, params.offset.y);\n"
    "  out.uv2 = in.pos;\n"
    "  return out;\n"
    "}\n";
static const char* fsq_fs =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float2 uv0;\n"
    "  float2 uv1;\n"
    "  float2 uv2;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]],\n"
    "  texture2d<float> tex0 [[texture(0)]], sampler smp0 [[sampler(0)]],\n"
    "  texture2d<float> tex1 [[texture(1)]], sampler smp1 [[sampler(1)]],\n"
    "  texture2d<float> tex2 [[texture(2)]], sampler smp2 [[sampler(2)]])\n"
    "{\n"
    "  float3 c0 = tex0.sample(smp0, in.uv0).xyz;\n"
    "  float3 c1 = tex1.sample(smp1, in.uv1).xyz;\n"
    "  float3 c2 = tex2.sample(smp2, in.uv2).xyz;\n"
    "  return float4(c0 + c1 + c2, 1.0);\n"
    "}\n";
static const char* dbg_vs =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct vs_in {\n"
    "  float2 pos [[attribute(0)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float2 uv;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]]) {\n"
    "  vs_out out;\n"
    "  out.pos = float4(in.pos*2.0-1.0, 0.5, 1.0);\n"
    "  out.uv = in.pos;\n"
    "  return out;\n"
    "}\n";
static const char* dbg_fs =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "fragment float4 _main(float2 uv [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return float4(tex.sample(smp, uv).xyz, 1.0);\n"
    "}\n";
#elif defined(SOKOL_D3D11)
static const char* cube_vs =
    "cbuffer params: register(b0) {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 pos: POSITION;\n"
    "  float bright: BRIGHT;\n"
    "};\n"
    "struct vs_out {\n"
    "  float bright: BRIGHT;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = mul(mvp, inp.pos);\n"
    "  outp.bright = inp.bright;\n"
    "  return outp;\n"
    "}\n";
static const char* cube_fs =
    "struct fs_out {\n"
    "  float4 c0: SV_Target0;\n"
    "  float4 c1: SV_Target1;\n"
    "  float4 c2: SV_Target2;\n"
    "};\n"
    "fs_out main(float b: BRIGHT) {\n"
    "  fs_out outp;\n"
    "  outp.c0 = float4(b, 0.0, 0.0, 1.0);\n"
    "  outp.c1 = float4(0.0, b, 0.0, 1.0);\n"
    "  outp.c2 = float4(0.0, 0.0, b, 1.0);\n"
    "  return outp;\n"
    "}\n";
static const char* fsq_vs =
    "cbuffer params {\n"
    "  float2 offset;\n"
    "};\n"
    "struct vs_in {\n"
    "  float2 pos: POSITION;\n"
    "};\n"
    "struct vs_out {\n"
    "  float2 uv0: TEXCOORD0;\n"
    "  float2 uv1: TEXCOORD1;\n"
    "  float2 uv2: TEXCOORD2;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = float4(inp.pos*2.0-1.0, 0.5, 1.0);\n"
    "  outp.uv0 = inp.pos + float2(offset.x, 0.0);\n"
    "  outp.uv1 = inp.pos + float2(0.0, offset.y);\n"
    "  outp.uv2 = inp.pos;\n"
    "  return outp;\n"
    "}\n";
static const char* fsq_fs =
    "Texture2D<float4> tex0: register(t0);\n"
    "Texture2D<float4> tex1: register(t1);\n"
    "Texture2D<float4> tex2: register(t2);\n"
    "sampler smp0: register(s0);\n"
    "sampler smp1: register(s1);\n"
    "sampler smp2: register(s2);\n"
    "struct fs_in {\n"
    "  float2 uv0: TEXCOORD0;\n"
    "  float2 uv1: TEXCOORD1;\n"
    "  float2 uv2: TEXCOORD2;\n"
    "};\n"
    "float4 main(fs_in inp): SV_Target0 {\n"
    "  float3 c0 = tex0.Sample(smp0, inp.uv0).xyz;\n"
    "  float3 c1 = tex1.Sample(smp1, inp.uv1).xyz;\n"
    "  float3 c2 = tex2.Sample(smp2, inp.uv2).xyz;\n"
    "  float4 c = float4(c0 + c1 + c2, 1.0);\n"
    "  return c;\n"
    "}\n";
static const char* dbg_vs =
    "struct vs_in {\n"
    "  float2 pos: POSITION;\n"
    "};\n"
    "struct vs_out {\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = float4(inp.pos*2.0-1.0, 0.5, 1.0);\n"
    "  outp.uv = inp.pos;\n"
    "  return outp;\n"
    "}\n";
static const char* dbg_fs =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0): SV_Target0 {\n"
    "  return float4(tex.Sample(smp, uv).xyz, 1.0);\n"
    "}\n";
#endif
