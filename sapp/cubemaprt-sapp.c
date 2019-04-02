//------------------------------------------------------------------------------
//  cubemaprt-sapp.c
//  Cubemap as render target.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "ui/dbgui.h"
#include <stddef.h> /* offsetof */

#define DISPLAY_SAMPLE_COUNT (4)
#define OFFSCREEN_SAMPLE_COUNT (1)
#define NUM_SHAPES (32)

typedef struct {
    hmm_mat4 model;
    hmm_vec4 color;
    hmm_vec3 axis;
    float radius;
    float angle;
    float angular_velocity;
} shape_t;

typedef struct {
    float pos[3];
    float norm[3];
} vertex_t;

typedef struct {
    hmm_mat4 mvp;
    hmm_mat4 model;
    hmm_vec4 shape_color;
    hmm_vec4 light_dir;
    hmm_vec4 eye_pos;
} shape_uniforms_t;

typedef struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
} mesh_t;

typedef struct {
    sg_image cubemap;
    sg_pass offscreen_pass[SG_CUBEFACE_NUM];
    sg_pass_action offscreen_pass_action;
    sg_pass_action display_pass_action;
    mesh_t cube;
    sg_pipeline offscreen_pip;
    sg_pipeline display_pip;
    hmm_mat4 offscreen_proj;
    shape_t shapes[NUM_SHAPES];
} app_t;
static app_t app;

static const char* vs_src, *offscreen_fs_src, *display_fs_src;

static mesh_t make_cube_mesh(void);

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
    __dbgui_setup(DISPLAY_SAMPLE_COUNT);

    /* create a cubemap as render target, and a matching depth-buffer texture */
    app.cubemap = sg_make_image(&(sg_image_desc){
        .type = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width = 1024,
        .height = 1024,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .label = "cubemap-color-rt"
    });
    /* ... and a matching depth-buffer image */
    sg_image depth_img = sg_make_image(&(sg_image_desc){
        .type = SG_IMAGETYPE_2D,
        .render_target = true,
        .width = 1024,
        .height = 1024,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .label = "cubemap-depth-rt"
    });

    /* create 6 pass objects, one for each cubemap face */
    for (int i = 0; i < SG_CUBEFACE_NUM; i++) {
        app.offscreen_pass[i] = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0] = {
                .image = app.cubemap,
                .face = i
            },
            .depth_stencil_attachment.image = depth_img,
            .label = "offscreen-pass"
        });
    }

    /* vertex- and index-buffers for cube  */
    app.cube = make_cube_mesh();

    /* shader and pipeline objects for offscreen-rendering */
    sg_shader_uniform_block_desc ub_desc = {
        .uniforms = {
            [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 },
            [1] = { .name="model", .type=SG_UNIFORMTYPE_MAT4 },
            [2] = { .name="shape_color", .type=SG_UNIFORMTYPE_FLOAT4 },
            [3] = { .name="light_dir", .type=SG_UNIFORMTYPE_FLOAT4 },
            [4] = { .name="eye_pos", .type=SG_UNIFORMTYPE_FLOAT4 }
        }
    };
    sg_layout_desc layout_desc = {
        .attrs = {
            [0] = { .name="pos", .sem_name="POS", .offset=offsetof(vertex_t, pos), .format=SG_VERTEXFORMAT_FLOAT3 },
            [1] = { .name="norm", .sem_name="NORM", .offset=offsetof(vertex_t, norm), .format=SG_VERTEXFORMAT_FLOAT3 }
        }
    };
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .source = vs_src,
            .uniform_blocks[0] = ub_desc
        },
        .fs.source = offscreen_fs_src,
        .label = "offscreen-shader"
    });
    app.offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = offscreen_shd,
        .layout = layout_desc,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true,
        },
        .blend = {
            .depth_format = SG_PIXELFORMAT_DEPTH,
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = OFFSCREEN_SAMPLE_COUNT
        },
        .label = "offscreen-pipeline"
    });

    /* shader and pipeline objects for display-rendering */
    sg_shader display_shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .source = vs_src,
            .uniform_blocks[0] = ub_desc
        },
        .fs = {
            .source = display_fs_src,
            .images[0] = {
                .name = "tex",
                .type = SG_IMAGETYPE_CUBE
            }
        }
    });
    app.display_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = display_shd,
        .layout = layout_desc,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true,
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK,
            .sample_count = DISPLAY_SAMPLE_COUNT
        }
    });
    
    /* 1:1 aspect ration projection matrix for offscreen rendering */
    app.offscreen_proj = HMM_Perspective(90.0f, 1.0f, 0.01f, 100.0f);
}

void frame(void) {
    sg_begin_default_pass(&app.display_pass_action, sapp_width(), sapp_height());
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
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = DISPLAY_SAMPLE_COUNT,
        .gl_force_gles2 = true,
        .window_title = "CubemapRenderTarget (sokol-app)",
    };
}

static mesh_t make_cube_mesh(void) {
    vertex_t vertices[] =  {
        { { -1.0, -1.0, -1.0 }, { 0.0, 0.0, -1.0 } },
        { {  1.0, -1.0, -1.0 }, { 0.0, 0.0, -1.0 } },
        { {  1.0,  1.0, -1.0 }, { 0.0, 0.0, -1.0 } },
        { { -1.0,  1.0, -1.0 }, { 0.0, 0.0, -1.0 } },

        { { -1.0, -1.0,  1.0 }, { 0.0, 0.0, 1.0 } },
        { {  1.0, -1.0,  1.0 }, { 0.0, 0.0, 1.0 } },
        { {  1.0,  1.0,  1.0 }, { 0.0, 0.0, 1.0 } },
        { { -1.0,  1.0,  1.0 }, { 0.0, 0.0, 1.0 } },

        { { -1.0, -1.0, -1.0 }, { -1.0, 0.0, 0.0 } },
        { { -1.0,  1.0, -1.0 }, { -1.0, 0.0, 0.0 } },
        { { -1.0,  1.0,  1.0 }, { -1.0, 0.0, 0.0 } },
        { { -1.0, -1.0,  1.0 }, { -1.0, 0.0, 0.0 } },

        { { 1.0, -1.0, -1.0, }, { 1.0, 0.0, 0.0 } },
        { { 1.0,  1.0, -1.0, }, { 1.0, 0.0, 0.0 } },
        { { 1.0,  1.0,  1.0, }, { 1.0, 0.0, 0.0 } },
        { { 1.0, -1.0,  1.0, }, { 1.0, 0.0, 0.0 } },

        { { -1.0, -1.0, -1.0 }, { 0.0, -1.0, 0.0 } },
        { { -1.0, -1.0,  1.0 }, { 0.0, -1.0, 0.0 } },
        { {  1.0, -1.0,  1.0 }, { 0.0, -1.0, 0.0 } },
        { {  1.0, -1.0, -1.0 }, { 0.0, -1.0, 0.0 } },

        { { -1.0,  1.0, -1.0 }, { 0.0, 1.0, 0.0 } },
        { { -1.0,  1.0,  1.0 }, { 0.0, 1.0, 0.0 } },
        { {  1.0,  1.0,  1.0 }, { 0.0, 1.0, 0.0 } },
        { {  1.0,  1.0, -1.0 }, { 0.0, 1.0, 0.0 } }
    };
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    mesh_t mesh = {
        .vbuf = sg_make_buffer(&(sg_buffer_desc){
            .size = sizeof(vertices),
            .content = vertices,
            .label = "cube-vertices"
        }),
        .ibuf = sg_make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .size = sizeof(indices),
            .content = indices,
            .label = "cube-indices"
        })
    };
    return mesh;
}

#if defined(SOKOL_GLCORE33)
#error "FIXME!"
#elif defined(SOKOL_GLES3) || defined(SOKOL_GLES2)
#error "FIXME!"
#elif defined(SOKOL_METAL)
static const char* vs_src =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"using namespace metal;\n"
"struct vs_params {\n"
"  float4x4 mvp;\n"
"  float4x4 model;\n"
"  float4 shape_color;\n"
"  float4 light_dir;\n"
"  float4 eye_pos;\n"
"};\n"
"struct vs_in {\n"
"  float4 pos [[attribute(0)]];\n"
"  float3 norm [[attribute(1)]];\n"
"};\n"
"struct vs_out {\n"
"  float3 world_position [[user(locn0)]];\n"
"  float3 world_normal [[user(locn1)]];\n"
"  float3 world_light_dir [[user(locn2)]];\n"
"  float3 world_eye_pos [[user(locn3)]];\n"
"  float4 color [[user(locn4)]];\n"
"  float4 pos [[position]];\n"
"};\n"
"vertex vs_out _main(vs_in in [[stage_in]], constant vs_params& params [[buffer(0)]]) {\n"
"  vs_out out;\n"
"  out.pos = params.mvp * in.pos;\n"
"  out.world_position = float4(params.model * in.pos).xyz;\n"
"  out.world_normal = float4(params.model * float4(in.norm, 0.0f)).xyz;\n"
"  out.world_eye_pos = params.eye_pos.xyz;\n"
"  out.world_light_dir = params.light_dir.xyz;\n"
"  out.color = params.shape_color;\n"
"  return out;\n"
"}\n";
static const char* offscreen_fs_src =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"using namespace metal;\n"
"struct fs_in {\n"
"  float3 world_position [[user(locn0)]];\n"
"  float3 world_normal [[user(locn1)]];\n"
"  float3 world_light_dir [[user(locn2)]];\n"
"  float3 world_eye_pos [[user(locn3)]];\n"
"  float4 color [[user(locn4)]];\n"
"};\n"
"float3 light(float3 base_color, float3 eye_vec, float3 normal, float3 light_vec) {\n"
"  float ambient = 0.25f;\n"
"  float n_dot_l = max(dot(normal, light_vec), 0.0);\n"
"  float diff = n_dot_l + ambient;\n"
"  float spec_power = 16.0;\n"
"  float3 r = reflect(-light_vec, normal);\n"
"  float r_dot_v = max(dot(r, eye_vec), 0.0);\n"
"  float spec = pow(r_dot_v, spec_power) * n_dot_l;\n"
"  return (base_color * (diff + ambient)) + float3(spec, spec, spec);\n"
"}\n"
"fragment float4 _main(fs_in in [[stage_in]]) {\n"
"  float3 nrm = normalize(in.world_normal);\n"
"  float3 eye_vec = normalize(in.world_eye_pos - in.world_position);\n"
"  float3 light_dir = normalize(in.world_light_dir);\n"
"  float3 frag_color = light(in.color.xyz, eye_vec, nrm, light_dir);\n"
"  return float4(frag_color, 1.0f);\n"
"};\n";
#elif defined(SOKOL_D3D11)
#error "FIXME!"
#endif
