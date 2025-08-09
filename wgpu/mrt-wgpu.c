//------------------------------------------------------------------------------
//  mrt-wgpu.c
//  Rendering with multi-rendertargets, and recreating render targets
//  when window size changes.
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"

#define NUM_MRTS (3)
#define OFFSCREEN_DEPTH_FORMAT SG_PIXELFORMAT_DEPTH
#define OFFSCREEN_COLOR_FORMAT SG_PIXELFORMAT_RGBA8
#define OFFSCREEN_SAMPLE_COUNT (4)
#define DISPLAY_MSAA_SAMPLES (1)


static struct {
    sg_image color_img[NUM_MRTS];
    sg_image resolve_img[NUM_MRTS];
    sg_image depth_img;
    struct {
        sg_pass pass;
        sg_attachments attachments;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
        sg_view resolve_texview[NUM_MRTS];
    } display;
    struct {
        sg_pipeline pip;
        sg_bindings bind;
    } dbg;
    sg_pass_action pass_action;
    float rx, ry;
} state;

typedef struct {
    float x, y, z, b;
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
} offscreen_params_t;

typedef struct {
    hmm_vec2 offset;
} fsq_params_t;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = wgpu_environment(),
        .logger.func = slog_func,
    });

    const int width = wgpu_width();
    const int height = wgpu_height();

    // image objects for the color-, resolve- and depth-stencil-attachments,
    // plus texture views on the resolve images
    for (int i = 0; i < NUM_MRTS; i++) {
        state.color_img[i] = sg_make_image(&(sg_image_desc){
            .usage.color_attachment = true,
            .width = width,
            .height = height,
            .pixel_format = OFFSCREEN_COLOR_FORMAT,
            .sample_count = OFFSCREEN_SAMPLE_COUNT,
        });
        state.resolve_img[i] = sg_make_image(&(sg_image_desc){
            .usage.resolve_attachment = true,
            .width = width,
            .height = height,
            .pixel_format = OFFSCREEN_COLOR_FORMAT,
            .sample_count = 1,
        });
        state.display.resolve_texview[i] = sg_make_view(&(sg_view_desc){
            .texture.image = state.resolve_img[i],
        });
    }
    state.depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = width,
        .height = height,
        .pixel_format = OFFSCREEN_DEPTH_FORMAT,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
    });

    // setup a pass struct for the offscreen render pass
    // (color-, resolve- and depth-stencil-attachment views, and pass action)
    state.offscreen.pass = (sg_pass) {
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
        .action = {
            .colors = {
                [0] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .store_action = SG_STOREACTION_DONTCARE,
                    .clear_value = { 0.25f, 0.0f, 0.0f, 1.0f },
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
                },
            },
        }
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
    sg_buffer cube_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices),
        .label = "cube-vertices",
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
    sg_buffer cube_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices),
        .label = "cube-indices",
    });

    // a shader to render a cube into MRT offscreen render targets
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct params {\n"
            "  mvp: mat4x4f,\n"
            "}\n"
            "@group(0) @binding(0) var<uniform> in: params;\n"
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) bright: f32,\n"
            "}\n"
            "@vertex fn main(@location(0) pos: vec4f, @location(1) bright: f32) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = in.mvp * pos;\n"
            "  out.bright = bright;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "struct fs_out {\n"
            "  @location(0) c0: vec4f,\n"
            "  @location(1) c1: vec4f,\n"
            "  @location(2) c2: vec4f,\n"
            "};\n"
            "@fragment fn main(@location(0) b: f32) -> fs_out {\n"
            "  var out: fs_out;\n"
            "  out.c0 = vec4f(b, 0, 0, 1);\n"
            "  out.c1 = vec4f(0, b, 0, 1);\n"
            "  out.c2 = vec4f(0, 0, b, 1);\n"
            "  return out;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(offscreen_params_t),
            .wgsl_group0_binding_n = 0,
        },
        .label = "offscreen-shader",
    });

    // pipeline object for the offscreen-rendered cube
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = sizeof(vertex_t),
            .attrs = {
                [0] = { .offset = offsetof(vertex_t,x), .format = SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .offset = offsetof(vertex_t,b), .format = SG_VERTEXFORMAT_FLOAT }
            }
        },
        .shader = offscreen_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .pixel_format = OFFSCREEN_DEPTH_FORMAT,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .color_count = 3,
        .colors = {
            [0].pixel_format = OFFSCREEN_COLOR_FORMAT,
            [1].pixel_format = OFFSCREEN_COLOR_FORMAT,
            [2].pixel_format = OFFSCREEN_COLOR_FORMAT,
        },
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "offscreen-pipeline",
    });

    // resource bindings for offscreen rendering
    state.offscreen.bind = (sg_bindings){
        .vertex_buffers[0] = cube_vbuf,
        .index_buffer = cube_ibuf
    };

    // a vertex buffer to render a fullscreen rectangle
    float quad_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quad_buf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices)
    });

    // a sampler object with linear filtering and clamp-to-edge
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "sampler",
    });

    // a shader to render a fullscreen rectangle, which 'composes'
    // the 3 offscreen render target images onto the screen
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct params {\n"
            "  offset: vec2f,\n"
            "}\n"
            "@group(0) @binding(0) var<uniform> in: params;\n"
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) uv0: vec2f,\n"
            "  @location(1) uv1: vec2f,\n"
            "  @location(2) uv2: vec2f,\n"
            "}\n"
            "@vertex fn main(@location(0) pos: vec2f) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = vec4f((pos * 2 - 1), 0.5, 1);\n"
            "  out.uv0 = pos + vec2f(in.offset.x, 0);\n"
            "  out.uv1 = pos + vec2f(0, in.offset.y);\n"
            "  out.uv2 = pos;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "@group(1) @binding(0) var tex0: texture_2d<f32>;\n"
            "@group(1) @binding(1) var tex1: texture_2d<f32>;\n"
            "@group(1) @binding(2) var tex2: texture_2d<f32>;\n"
            "@group(1) @binding(3) var smp: sampler;\n"
            "@fragment fn main(@location(0) uv0: vec2f, @location(1) uv1: vec2f, @location(2) uv2: vec2f) -> @location(0) vec4f {\n"
            "  var c0 = textureSample(tex0, smp, uv0).xyz;\n"
            "  var c1 = textureSample(tex1, smp, uv1).xyz;\n"
            "  var c2 = textureSample(tex2, smp, uv2).xyz;\n"
            "  var c = vec4f(c0 + c1 + c2, 1);\n"
            "  return c;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(fsq_params_t),
            .wgsl_group0_binding_n = 0,
        },
        .views = {
            [0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .wgsl_group1_binding_n = 0 },
            [1].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .wgsl_group1_binding_n = 1 },
            [2].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .wgsl_group1_binding_n = 2 },
        },
        .samplers = {
            [0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .wgsl_group1_binding_n = 3 },
        },
        .texture_sampler_pairs = {
            [0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 0, .sampler_slot = 0 },
            [1] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 1, .sampler_slot = 0 },
            [2] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 2, .sampler_slot = 0 },
        },
        .label = "fsq-shader",
    });

    // the pipeline object for the fullscreen rectangle
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "fsq-pipeline"
    });

    // resource bindings for the fullscreen quad
    state.display.bind = (sg_bindings){
        .vertex_buffers[0] = quad_buf,
        .views = {
            [0] = state.display.resolve_texview[0],
            [1] = state.display.resolve_texview[1],
            [2] = state.display.resolve_texview[2],
        },
        .samplers[0] = smp,
    };

    // shader, pipeline and resource bindings for debug-visualization quads
    sg_shader dbg_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) uv: vec2f,\n"
            "}\n"
            "@vertex fn main(@location(0) pos: vec2f) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  out.pos = vec4f(pos * 2 - 1, 0.5, 1);\n"
            "  out.uv = pos;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "@group(1) @binding(0) var tex0: texture_2d<f32>;\n"
            "@group(1) @binding(1) var smp: sampler;\n"
            "@fragment fn main(@location(0) uv: vec2f) -> @location(0) vec4f {\n"
            "  return vec4f(textureSample(tex0, smp, uv).xyz, 1);\n"
            "}\n",
        .views[0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .wgsl_group1_binding_n = 0 },
        .samplers[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .wgsl_group1_binding_n = 1 },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
        },
        .label = "dbg-shader",
    });
    state.dbg.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .shader = dbg_shd,
        .label = "dbg-pipeline"
    });
    // images will be filled right before rendering
    state.dbg.bind = (sg_bindings){
        .vertex_buffers[0] = quad_buf,
        .samplers[0] = smp,
    };
}

static void frame(void) {

    const int width = wgpu_width();
    const int height = wgpu_height();

    // view-projection matrix for the offscreen-rendered cube
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)width/(float)height, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // shader parameters
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    const offscreen_params_t offscreen_params = {
        .mvp = HMM_MultiplyMat4(view_proj, model),
    };
    const fsq_params_t fsq_params = {
        .offset = HMM_Vec2(HMM_SinF(state.rx * 0.01f) * 0.1f, HMM_SinF(state.ry * 0.01f) * 0.1f),
    };

    // render cube into MRT offscreen render targets
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_apply_uniforms(0, &SG_RANGE(offscreen_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    // render fullscreen quad with the 'composed image', plus 3 small debug-view quads
    sg_begin_pass(&(sg_pass){
        .action = state.pass_action,
        .swapchain = wgpu_swapchain()
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(0, &SG_RANGE(fsq_params));
    sg_draw(0, 4, 1);
    sg_apply_pipeline(state.dbg.pip);
    for (int i = 0; i < 3; i++) {
        sg_apply_viewport(i*100, 0, 100, 100, false);
        state.dbg.bind.views[0] = state.display.resolve_texview[i];
        sg_apply_bindings(&state.dbg.bind);
        sg_draw(0, 4, 1);
    }
    sg_apply_viewport(0, 0, width, height, false);
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
        .width = 800,
        .height = 600,
        .sample_count = DISPLAY_MSAA_SAMPLES,
        .title = "mrt-wgpu"
    });
    return 0;
}
