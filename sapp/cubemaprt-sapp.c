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

/* change the OFFSCREEN_SAMPLE_COUNT between 1 and 4 to test the
   different cubemap-rendering-paths in sokol (one rendering to a
   separate MSAA surface, and MSAA-resolve in sg_end_pass(), and
   the other (without MSAA) rendering directly to the cubemap faces
*/
#define OFFSCREEN_SAMPLE_COUNT (4)
#define DISPLAY_SAMPLE_COUNT (4)
#define NUM_SHAPES (32)

/* state struct for the little cubes rotating around the big cube */
typedef struct {
    hmm_mat4 model;
    hmm_vec4 color;
    hmm_vec3 axis;
    float radius;
    float angle;
    float angular_velocity;
} shape_t;

/* vertex (normals for simple point lighting) */
typedef struct {
    float pos[3];
    float norm[3];
} vertex_t;

/* shader-uniform block */
typedef struct {
    hmm_mat4 mvp;
    hmm_mat4 model;
    hmm_vec4 shape_color;
    hmm_vec4 light_dir;
    hmm_vec4 eye_pos;
} shape_uniforms_t;

/* a mesh consists of a vertex- and index-buffer */
typedef struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    int num_elements;
} mesh_t;

/* the entire application state */
typedef struct {
    sg_image cubemap;
    sg_pass offscreen_pass[SG_CUBEFACE_NUM];
    sg_pass_action offscreen_pass_action;
    sg_pass_action display_pass_action;
    mesh_t cube;
    sg_pipeline offscreen_shapes_pip;
    sg_pipeline display_shapes_pip;
    sg_pipeline display_pip;
    hmm_mat4 offscreen_proj;
    hmm_vec4 light_dir;
    float rx, ry;
    shape_t shapes[NUM_SHAPES];
} app_t;
static app_t app;

static const char* vs_src, *offscreen_fs_src, *display_fs_src;

static void draw_cubes(sg_pipeline pip, hmm_vec3 eye_pos, hmm_mat4 view_proj);
static mesh_t make_cube_mesh(void);

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static inline float rnd(float min_val, float max_val) {
    return ((((float)(xorshift32() & 0xFFFF)) / 0x10000) * (max_val - min_val)) + min_val;
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

    /* pass action for offscreen pass (clear to black) */
    app.offscreen_pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.5f, 0.5f, 0.5f, 1.0f } }
    };
    /* pass action for default pass (clear to grey) */
    app.display_pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.75f, 0.75f, 0.75f, 1.0f } }
    };

    /* vertex- and index-buffers for cube  */
    app.cube = make_cube_mesh();

    /* shader and pipeline objects for offscreen-rendering */
    sg_shader_uniform_block_desc ub_desc = {
        .size = sizeof(shape_uniforms_t),
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
            [0] = { .offset=offsetof(vertex_t, pos), .format=SG_VERTEXFORMAT_FLOAT3 },
            [1] = { .offset=offsetof(vertex_t, norm), .format=SG_VERTEXFORMAT_FLOAT3 }
        }
    };
    sg_shader offscreen_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0] = { .name="pos", .sem_name="POS" },
            [1] = { .name="norm", .sem_name="NORM" }
        },
        .vs = {
            .source = vs_src,
            .uniform_blocks[0] = ub_desc
        },
        .fs.source = offscreen_fs_src,
        .label = "offscreen-shader"
    });
    sg_pipeline_desc pip_desc = {
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
        .label = "offscreen-shapes-pipeline"
    };
    app.offscreen_shapes_pip = sg_make_pipeline(&pip_desc);
    pip_desc.rasterizer.sample_count = DISPLAY_SAMPLE_COUNT;
    pip_desc.blend.depth_format = 0;
    pip_desc.label = "display-shapes-pipeline";
    app.display_shapes_pip = sg_make_pipeline(&pip_desc);

    /* shader and pipeline objects for display-rendering */
    sg_shader display_shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0] = { .name="pos", .sem_name="POS" },
            [1] = { .name="norm", .sem_name="NORM" }
        },
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
    app.light_dir = HMM_Vec4v(HMM_NormalizeVec3(HMM_Vec3(-0.75f, 1.0f, 0.0f)), 0.0f);

    /* setup initial state for the orbiting cubes */
    for (int i = 0; i < NUM_SHAPES; i++) {
        app.shapes[i].color = HMM_Vec4(rnd(0.0f, 1.0f), rnd(0.0f, 1.0f), rnd(0.0f, 1.0f), 1.0f);
        app.shapes[i].axis = HMM_NormalizeVec3(HMM_Vec3(rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f)));
        app.shapes[i].radius = rnd(5.0f, 10.0f);
        app.shapes[i].angle = rnd(0.0f, 360.0f);
        app.shapes[i].angular_velocity = rnd(15.0f, 50.0f) * (rnd(-1.0f, 1.0f)>0.0f ? 1.0f : -1.0f);
    }
}

void frame(void) {

    /* update the little cubes that are reflected in the big cube */
    for (int i = 0; i < NUM_SHAPES; i++) {
        app.shapes[i].angle += app.shapes[i].angular_velocity * (1.0f/60.0f);
        hmm_mat4 scale = HMM_Scale(HMM_Vec3(0.25f, 0.25f, 0.25f));
        hmm_mat4 rot = HMM_Rotate(app.shapes[i].angle, app.shapes[i].axis);
        hmm_mat4 trans = HMM_Translate(HMM_Vec3(0.0f, 0.0f, app.shapes[i].radius));
        app.shapes[i].model = HMM_MultiplyMat4(rot, HMM_MultiplyMat4(trans, scale));
    }

    /* offscreen pass which renders the environment cubemap */
    /* FIXME: these values work for Metal and D3D11, not for GL, because
       of the different handedness of the cubemap coordinate systems
    */
    #if defined(SOKOL_METAL) || defined(SOKOL_D3D11)
    hmm_vec3 center_and_up[SG_CUBEFACE_NUM][2] = {
        { { .X=+1.0f, .Y= 0.0f, .Z= 0.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } },
        { { .X=-1.0f, .Y= 0.0f, .Z= 0.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } },
        { { .X= 0.0f, .Y=-1.0f, .Z= 0.0f }, { .X=0.0f, .Y= 0.0f, .Z=-1.0f } },
        { { .X= 0.0f, .Y=+1.0f, .Z= 0.0f }, { .X=0.0f, .Y= 0.0f, .Z=+1.0f } },
        { { .X= 0.0f, .Y= 0.0f, .Z=+1.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } },
        { { .X= 0.0f, .Y= 0.0f, .Z=-1.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } }
    };
    #else /* GL */
    hmm_vec3 center_and_up[SG_CUBEFACE_NUM][2] = {
        { { .X=+1.0f, .Y= 0.0f, .Z= 0.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } },
        { { .X=-1.0f, .Y= 0.0f, .Z= 0.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } },
        { { .X= 0.0f, .Y=+1.0f, .Z= 0.0f }, { .X=0.0f, .Y= 0.0f, .Z=+1.0f } },
        { { .X= 0.0f, .Y=-1.0f, .Z= 0.0f }, { .X=0.0f, .Y= 0.0f, .Z=-1.0f } },
        { { .X= 0.0f, .Y= 0.0f, .Z=+1.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } },
        { { .X= 0.0f, .Y= 0.0f, .Z=-1.0f }, { .X=0.0f, .Y=-1.0f, .Z= 0.0f } }
    };
    #endif
    for (int face = 0; face < SG_CUBEFACE_NUM; face++) {
        sg_begin_pass(app.offscreen_pass[face], &app.offscreen_pass_action);
        hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 0.0f), center_and_up[face][0], center_and_up[face][1]);
        hmm_mat4 view_proj = HMM_MultiplyMat4(app.offscreen_proj, view);
        draw_cubes(app.offscreen_shapes_pip, HMM_Vec3(0.0f, 0.0f, 0.0f), view_proj);
        sg_end_pass();
    }

    /* render the default pass */
    const int w = sapp_width();
    const int h = sapp_height();
    sg_begin_default_pass(&app.display_pass_action, sapp_width(), sapp_height());

    hmm_vec3 eye_pos = HMM_Vec3(0.0f, 0.0f, 30.0f);
    hmm_mat4 proj = HMM_Perspective(45.0f, (float)w/(float)h, 0.01f, 100.0f);
    hmm_mat4 view = HMM_LookAt(eye_pos, HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    /* render the orbiting cubes */
    draw_cubes(app.display_shapes_pip, eye_pos, view_proj);

    /* render a big cube in the middle with environment mapping */
    app.rx += 0.1f; app.ry += 0.2f;
    hmm_mat4 rxm = HMM_Rotate(app.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(app.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(HMM_MultiplyMat4(rxm, rym), HMM_Scale(HMM_Vec3(2.0f, 2.0f, 2.f)));
    sg_apply_pipeline(app.display_pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = app.cube.vbuf,
        .index_buffer = app.cube.ibuf,
        .fs_images[0] = app.cubemap
    });
    shape_uniforms_t uniforms = {
        .mvp = HMM_MultiplyMat4(view_proj, model),
        .model = model,
        .shape_color = HMM_Vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .light_dir = app.light_dir,
        .eye_pos = HMM_Vec4v(eye_pos, 1.0f)
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
    sg_draw(0, app.cube.num_elements, 1);

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
        .window_title = "Cubemap Render Target (sokol-app)",
    };
}

static void draw_cubes(sg_pipeline pip, hmm_vec3 eye_pos, hmm_mat4 view_proj) {
    sg_apply_pipeline(pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = app.cube.vbuf,
        .index_buffer = app.cube.ibuf
    });
    for (int i = 0; i < NUM_SHAPES; i++) {
        const shape_t* shape = &app.shapes[i];
        shape_uniforms_t uniforms = {
            .mvp = HMM_MultiplyMat4(view_proj, shape->model),
            .model = shape->model,
            .shape_color = shape->color,
            .light_dir = app.light_dir,
            .eye_pos = HMM_Vec4v(eye_pos, 1.0f)
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
        sg_draw(0, app.cube.num_elements, 1);
    }
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
        }),
        .num_elements = sizeof(indices) / sizeof(uint16_t)
    };
    return mesh;
}

#if defined(SOKOL_GLCORE33)
static const char* vs_src =
"#version 330\n"
"uniform mat4 mvp;\n"
"uniform mat4 model;\n"
"uniform vec4 shape_color;\n"
"uniform vec4 light_dir;\n"
"uniform vec4 eye_pos;\n"
"in vec4 pos;\n"
"in vec3 norm;\n"
"out vec3 world_position;\n"
"out vec3 world_normal;\n"
"out vec3 world_eyepos;\n"
"out vec3 world_lightdir;\n"
"out vec4 color;\n"
"void main() {\n"
"  gl_Position = mvp * pos;\n"
"  world_position = vec4(model * pos).xyz;\n"
"  world_normal = vec4(model * vec4(norm, 0.0)).xyz;\n"
"  world_eyepos = eye_pos.xyz;\n"
"  world_lightdir = light_dir.xyz;\n"
"  color = shape_color;\n"
"}\n";
static const char* offscreen_fs_src =
"#version 330\n"
"in vec3 world_normal;\n"
"in vec3 world_position;\n"
"in vec3 world_eyepos;\n"
"in vec3 world_lightdir;\n"
"in vec4 color;\n"
"out vec4 frag_color;\n"
"vec3 light(vec3 base_color, vec3 eye_vec, vec3 normal, vec3 light_vec) {\n"
"  float ambient = 0.25;\n"
"  float n_dot_l = max(dot(normal, light_vec), 0.0);\n"
"  float diff = n_dot_l + ambient;\n"
"  float spec_power = 16.0;\n"
"  vec3 r = reflect(-light_vec, normal);\n"
"  float r_dot_v = max(dot(r, eye_vec), 0.0);\n"
"  float spec = pow(r_dot_v, spec_power) * n_dot_l;"
"  return base_color * (diff+ambient) + vec3(spec,spec,spec);"
"}\n"
"void main() {\n"
"  vec3 eye_vec = normalize(world_eyepos - world_position);\n"
"  vec3 nrm = normalize(world_normal);\n"
"  vec3 light_dir = normalize(world_lightdir);\n"
"  frag_color = vec4(light(color.xyz, eye_vec, nrm, light_dir), 1.0);\n"
"}\n";
static const char* display_fs_src =
"#version 330\n"
"uniform samplerCube tex;\n"
"in vec3 world_normal;\n"
"in vec3 world_position;\n"
"in vec3 world_eyepos;\n"
"in vec3 world_lightdir;\n"
"in vec4 color;\n"
"out vec4 frag_color;\n"
"vec3 light(vec3 base_color, vec3 eye_vec, vec3 normal, vec3 light_vec) {\n"
"  float ambient = 0.25;\n"
"  float n_dot_l = max(dot(normal, light_vec), 0.0);\n"
"  float diff = n_dot_l + ambient;\n"
"  float spec_power = 16.0;\n"
"  vec3 r = reflect(-light_vec, normal);\n"
"  float r_dot_v = max(dot(r, eye_vec), 0.0);\n"
"  float spec = pow(r_dot_v, spec_power) * n_dot_l;"
"  return base_color * (diff+ambient) + vec3(spec,spec,spec);"
"}\n"
"void main() {\n"
"  vec3 eye_vec = normalize(world_eyepos - world_position);\n"
"  vec3 nrm = normalize(world_normal);\n"
"  vec3 light_dir = normalize(world_lightdir);\n"
"  vec3 refl_vec = normalize(world_position);\n"
"  vec3 refl_color = texture(tex, refl_vec).xyz;\n"
"  frag_color = vec4(light(refl_color * color.xyz, eye_vec, nrm, light_dir), 1.0);\n"
"}\n";
#elif defined(SOKOL_GLES3) || defined(SOKOL_GLES2)
static const char* vs_src =
"uniform mat4 mvp;\n"
"uniform mat4 model;\n"
"uniform vec4 shape_color;\n"
"uniform vec4 light_dir;\n"
"uniform vec4 eye_pos;\n"
"attribute vec4 pos;\n"
"attribute vec3 norm;\n"
"varying vec3 world_position;\n"
"varying vec3 world_normal;\n"
"varying vec3 world_eyepos;\n"
"varying vec3 world_lightdir;\n"
"varying vec4 color;\n"
"void main() {\n"
"  gl_Position = mvp * pos;\n"
"  world_position = vec4(model * pos).xyz;\n"
"  world_normal = vec4(model * vec4(norm, 0.0)).xyz;\n"
"  world_eyepos = eye_pos.xyz;\n"
"  world_lightdir = light_dir.xyz;\n"
"  color = shape_color;\n"
"}\n";
static const char* offscreen_fs_src =
"precision mediump float;\n"
"varying vec3 world_normal;\n"
"varying vec3 world_position;\n"
"varying vec3 world_eyepos;\n"
"varying vec3 world_lightdir;\n"
"varying vec4 color;\n"
"vec3 light(vec3 base_color, vec3 eye_vec, vec3 normal, vec3 light_vec) {\n"
"  float ambient = 0.25;\n"
"  float n_dot_l = max(dot(normal, light_vec), 0.0);\n"
"  float diff = n_dot_l + ambient;\n"
"  float spec_power = 16.0;\n"
"  vec3 r = reflect(-light_vec, normal);\n"
"  float r_dot_v = max(dot(r, eye_vec), 0.0);\n"
"  float spec = pow(r_dot_v, spec_power) * n_dot_l;"
"  return base_color * (diff+ambient) + vec3(spec,spec,spec);"
"}\n"
"void main() {\n"
"  vec3 eye_vec = normalize(world_eyepos - world_position);\n"
"  vec3 nrm = normalize(world_normal);\n"
"  vec3 light_dir = normalize(world_lightdir);\n"
"  gl_FragColor = vec4(light(color.xyz, eye_vec, nrm, light_dir), 1.0);\n"
"}\n";
static const char* display_fs_src =
"precision mediump float;\n"
"uniform samplerCube tex;\n"
"varying vec3 world_normal;\n"
"varying vec3 world_position;\n"
"varying vec3 world_eyepos;\n"
"varying vec3 world_lightdir;\n"
"varying vec4 color;\n"
"vec3 light(vec3 base_color, vec3 eye_vec, vec3 normal, vec3 light_vec) {\n"
"  float ambient = 0.25;\n"
"  float n_dot_l = max(dot(normal, light_vec), 0.0);\n"
"  float diff = n_dot_l + ambient;\n"
"  float spec_power = 16.0;\n"
"  vec3 r = reflect(-light_vec, normal);\n"
"  float r_dot_v = max(dot(r, eye_vec), 0.0);\n"
"  float spec = pow(r_dot_v, spec_power) * n_dot_l;"
"  return base_color * (diff+ambient) + vec3(spec,spec,spec);"
"}\n"
"void main() {\n"
"  vec3 eye_vec = normalize(world_eyepos - world_position);\n"
"  vec3 nrm = normalize(world_normal);\n"
"  vec3 light_dir = normalize(world_lightdir);\n"
"  vec3 refl_vec = normalize(world_position);\n"
"  vec3 refl_color = textureCube(tex, refl_vec).xyz;\n"
"  gl_FragColor = vec4(light(refl_color * color.xyz, eye_vec, nrm, light_dir), 1.0);\n"
"}\n";
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
"}\n";
static const char* display_fs_src =
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
"fragment float4 _main(fs_in in [[stage_in]], texturecube<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
"  float3 eye_vec = normalize(in.world_eye_pos - in.world_position);\n"
"  float3 nrm = normalize(in.world_normal);\n"
"  float3 refl_vec = normalize(in.world_position).xyz * float3(1.0f, -1.0f, 1.0f);\n"
"  float4 refl_color = tex.sample(smp, refl_vec);\n"
"  float3 light_dir = normalize(in.world_light_dir);\n"
"  float3 frag_color = light(in.color.xyz * refl_color.xyz, eye_vec, nrm, light_dir);\n"
"  return float4(frag_color, 1.0f);\n"
"}\n";
#elif defined(SOKOL_D3D11)
static const char* vs_src =
"cbuffer params: register(b0) {\n"
"  float4x4 mvp;\n"
"  float4x4 model;\n"
"  float4 shape_color;\n"
"  float4 light_dir;\n"
"  float4 eye_pos;\n"
"};\n"
"struct vs_in {\n"
"  float4 pos: POS;\n"
"  float3 norm: NORM;\n"
"};\n"
"struct vs_out {\n"
"  float3 world_position: TEXCOORD0;\n"
"  float3 world_normal: TEXCOORD1;\n"
"  float3 world_light_dir: TEXCOORD2;\n"
"  float3 world_eye_pos: TEXCOORD3;\n"
"  float4 color: TEXCOORD4;\n"
"  float4 pos: SV_Position;\n"
"};\n"
"vs_out main(vs_in inp) {\n"
"  vs_out outp;\n"
"  outp.pos = mul(mvp, inp.pos);\n"
"  outp.world_position = mul(model, inp.pos).xyz;\n"
"  outp.world_normal = mul(model, float4(inp.norm, 0.0)).xyz;\n"
"  outp.world_eye_pos = eye_pos.xyz;\n"
"  outp.world_light_dir = light_dir.xyz;\n"
"  outp.color = shape_color;\n"
"  return outp;\n"
"};\n";
static const char* offscreen_fs_src =
"struct fs_in {\n"
"  float3 world_position: TEXCOORD0;\n"
"  float3 world_normal: TEXCOORD1;\n"
"  float3 world_light_dir: TEXCOORD2;\n"
"  float3 world_eye_pos: TEXCOORD3;\n"
"  float4 color: TEXCOORD4;\n"
"};\n"
"float3 light(float3 base_color, float3 eye_vec, float3 normal, float3 light_vec) {\n"
"  float ambient = 0.25;\n"
"  float n_dot_l = max(dot(normal, light_vec), 0.0);\n"
"  float diff = n_dot_l + ambient;\n"
"  float spec_power = 16.0;\n"
"  float3 r = reflect(-light_vec, normal);\n"
"  float r_dot_v = max(dot(r, eye_vec), 0.0);\n"
"  float spec = pow(r_dot_v, spec_power) * n_dot_l;\n"
"  return (base_color * (diff + ambient)) + float3(spec, spec, spec);\n"
"}\n"
"float4 main(fs_in inp): SV_Target0 {\n"
"  float3 nrm = normalize(inp.world_normal);\n"
"  float3 eye_vec = normalize(inp.world_eye_pos - inp.world_position);\n"
"  float3 light_dir = normalize(inp.world_light_dir);\n"
"  float3 frag_color = light(inp.color.xyz, eye_vec, nrm, light_dir);\n"
"  return float4(frag_color, 1.0);\n"
"}\n";
static const char* display_fs_src =
"TextureCube<float4> tex: register(t0);\n"
"sampler smp: register(s0);\n"
"struct fs_in {\n"
"  float3 world_position: TEXCOORD0;\n"
"  float3 world_normal: TEXCOORD1;\n"
"  float3 world_light_dir: TEXCOORD2;\n"
"  float3 world_eye_pos: TEXCOORD3;\n"
"  float4 color: TEXCOORD4;\n"
"};\n"
"float3 light(float3 base_color, float3 eye_vec, float3 normal, float3 light_vec) {\n"
"  float ambient = 0.25;\n"
"  float n_dot_l = max(dot(normal, light_vec), 0.0);\n"
"  float diff = n_dot_l + ambient;\n"
"  float spec_power = 16.0;\n"
"  float3 r = reflect(-light_vec, normal);\n"
"  float r_dot_v = max(dot(r, eye_vec), 0.0);\n"
"  float spec = pow(r_dot_v, spec_power) * n_dot_l;\n"
"  return (base_color * (diff + ambient)) + float3(spec, spec, spec);\n"
"}\n"
"float4 main(fs_in inp): SV_Target0 {\n"
"  float3 nrm = normalize(inp.world_normal);\n"
"  float3 eye_vec = normalize(inp.world_eye_pos - inp.world_position);\n"
"  float3 refl_vec = normalize(inp.world_position).xyz * float3(1.0f, -1.0f, 1.0f);\n"
"  float4 refl_color = tex.Sample(smp, refl_vec);\n"
"  float3 light_dir = normalize(inp.world_light_dir);\n"
"  float3 frag_color = light(inp.color.xyz * refl_color.xyz, eye_vec, nrm, light_dir);\n"
"  return float4(frag_color, 1.0);\n"
"}\n";
#endif
