//------------------------------------------------------------------------------
//  mrt-metal.c
//------------------------------------------------------------------------------
#include <stddef.h>     /* offsetof */
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

#define NUM_MRTS (3)
#define WIDTH (640)
#define HEIGHT (480)
#define SAMPLE_COUNT (4)

static struct {
    sg_image color_img[NUM_MRTS];
    sg_image resolve_img[NUM_MRTS];
    sg_image depth_img;
    struct {
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
    } fsq;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
        sg_pass_action pass_action;
    } display;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
    } dbg;
    hmm_mat4 view_proj;
    float rx;
    float ry;
} state;

typedef struct {
    float x, y, z, b;
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} offscreen_params_t;

typedef struct {
    hmm_vec2 offset;
} params_t;

static void init(void) {
    // setup sokol
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    // image objects for the various render pass attachments
    for (int i = 0; i < NUM_MRTS; i++) {
        state.color_img[i] = sg_make_image(&(sg_image_desc){
            .usage.color_attachment = true,
            .width = WIDTH,
            .height = HEIGHT,
            .sample_count = SAMPLE_COUNT,
        });
        state.resolve_img[i] = sg_make_image(&(sg_image_desc){
            .usage.resolve_attachment = true,
            .width = WIDTH,
            .height = HEIGHT,
            .sample_count = 1,
        });
    }
    state.depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = WIDTH,
        .height = HEIGHT,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = SAMPLE_COUNT,
    });

    // a pass struct with attachment view objects and the clear action
    state.offscreen.pass = (sg_pass){
        // pass attachment views
        .attachments = {
            .colors = {
                [0] = sg_make_view(&(sg_view_desc){ .color_attachment.image = state.color_img[0] }),
                [1] = sg_make_view(&(sg_view_desc){ .color_attachment.image = state.color_img[1] }),
                [2] = sg_make_view(&(sg_view_desc){ .color_attachment.image = state.color_img[2] }),
            },
            .resolves = {
                [0] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = state.resolve_img[0] }),
                [1] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = state.resolve_img[1] }),
                [2] = sg_make_view(&(sg_view_desc){ .resolve_attachment.image = state.resolve_img[2] }),
            },
            .depth_stencil = sg_make_view(&(sg_view_desc){ .depth_stencil_attachment.image = state.depth_img }),
        },
        // clear actions
        .action = {
            .colors = {
                [0] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.25f, 0.0f, 0.0f, 1.0f }
                },
                [1] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.0f, 0.25f, 0.0f, 1.0f }
                },
                [2] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.0f, 0.0f, 0.25f, 1.0f }
                }
            }
        },
    };

    // cube vertex buffer
    vertex_t cube_vertices[] = {
        // pos + brightness
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
    state.offscreen.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices)
    });

    // index buffer for the cube
    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.offscreen.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices)
    });

    // a shader to render the cube into offscreen MRT render targest
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
            "}\n",
        .fragment_func.source =
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
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(offscreen_params_t),
            .msl_buffer_n = 0,
        },
    });

    // pipeline object for the offscreen-rendered cube
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [0] = { .offset = offsetof(vertex_t,x), .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset = offsetof(vertex_t,b), .format=SG_VERTEXFORMAT_FLOAT }
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = SAMPLE_COUNT,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .color_count = 3,
    });

    // a vertex buffer to render a fullscreen rectangle
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices)
    });

    // a sampler with linear filtering and clamp-to-edge
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    // a shader to render a fullscreen rectangle by adding the 3 offscreen-rendered images
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
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
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct fs_in {\n"
            "  float2 uv0;\n"
            "  float2 uv1;\n"
            "  float2 uv2;\n"
            "};\n"
            "fragment float4 _main(fs_in in [[stage_in]],\n"
            "  texture2d<float> tex0 [[texture(0)]],\n"
            "  texture2d<float> tex1 [[texture(1)]],\n"
            "  texture2d<float> tex2 [[texture(2)]],\n"
            "  sampler smp [[sampler(0)]])\n"
            "{\n"
            "  float3 c0 = tex0.sample(smp, in.uv0).xyz;\n"
            "  float3 c1 = tex1.sample(smp, in.uv1).xyz;\n"
            "  float3 c2 = tex2.sample(smp, in.uv2).xyz;\n"
            "  return float4(c0 + c1 + c2, 1.0);\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(params_t),
            .msl_buffer_n = 0,
        },
        .views = {
            [0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_texture_n = 0 },
            [1].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_texture_n = 1 },
            [2].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_texture_n = 2 },
        },
        .samplers = {
            [0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_sampler_n = 0 },
        },
        .texture_sampler_pairs = {
            [0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 0, .sampler_slot = 0 },
            [1] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 1, .sampler_slot = 0 },
            [2] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 2, .sampler_slot = 0 },
        },
    });

    // setup resource binding struct for rendering fullscreen quad,
    // with the resolve attachment images bound as texture views
    state.display.bind = (sg_bindings) {
        .vertex_buffers[0] = quad_vbuf,
        .views[0] = sg_make_view(&(sg_view_desc){ .texture.image = state.resolve_img[0] }),
        .views[1] = sg_make_view(&(sg_view_desc){ .texture.image = state.resolve_img[1] }),
        .views[2] = sg_make_view(&(sg_view_desc){ .texture.image = state.resolve_img[2] }),
        .samplers[0] = smp,
    };

    // the pipeline object to render the fullscreen quad
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format=SG_VERTEXFORMAT_FLOAT2
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    // default pass action, no clear needed, since whole screen is overwritten
    state.display.pass_action = (sg_pass_action){
        .colors[0].load_action = SG_LOADACTION_DONTCARE,
        .depth.load_action = SG_LOADACTION_DONTCARE,
        .stencil.load_action = SG_LOADACTION_DONTCARE
    };

    // and a pipeline and resources to render debug-visualization quads with the content
    // of the offscreen render targets (images will be filled right before rendering
    state.dbg.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = sg_make_shader(&(sg_shader_desc){
            .vertex_func.source =
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
                "}\n",
            .fragment_func.source =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "fragment float4 _main(float2 uv [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
                "  return float4(tex.sample(smp, uv).xyz, 1.0);\n"
                "}\n",
            .views[0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_texture_n = 0 },
            .samplers[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_sampler_n = 0 },
            .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 0, .sampler_slot = 0 },
        }),
    });
    // images will be filled right before rendering
    state.dbg.bind = (sg_bindings){
        .vertex_buffers[0] = quad_vbuf,
        .samplers[0] = smp,
    };

    // view-projection matrix
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    state.view_proj = HMM_MultiplyMat4(proj, view);
}

static void frame(void) {
    offscreen_params_t offscreen_params;
    params_t params;
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    offscreen_params.mvp = HMM_MultiplyMat4(state.view_proj, model);
    params.offset = HMM_Vec2(HMM_SinF(state.rx*0.01f)*0.1f, HMM_SinF(state.ry*0.01f)*0.1f);

    // render cube into MRT offscreen render targets
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(0, &SG_RANGE(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // render fullscreen quad with the 'composed image'
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = osx_swapchain()
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(0, &SG_RANGE(params));
    sg_draw(0, 4, 1);

    // render the small debug quads
    sg_apply_pipeline(state.dbg.pip);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        state.dbg.bind.views[0] = state.display.bind.views[i],
        sg_apply_bindings(&state.dbg.bind);
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, SAMPLE_COUNT, SG_PIXELFORMAT_DEPTH, "mrt-metal", init, frame, shutdown);
    return 0;
}
