//------------------------------------------------------------------------------
//  cubemaprt-sapp.c
//  Cubemap as render target.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include <stddef.h> /* offsetof */
#include "cubemaprt-sapp.glsl.h"

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
    sg_pipeline display_cube_pip;
    hmm_mat4 offscreen_proj;
    hmm_vec4 light_dir;
    float rx, ry;
    shape_t shapes[NUM_SHAPES];
} app_t;
static app_t app;

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
        .context = sapp_sgcontext()
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

    /* same vertex layout for all shaders */
    sg_layout_desc layout = {
        .attrs = {
            [ATTR_vs_pos] = { .offset=offsetof(vertex_t, pos), .format=SG_VERTEXFORMAT_FLOAT3 },
            [ATTR_vs_norm] = { .offset=offsetof(vertex_t, norm), .format=SG_VERTEXFORMAT_FLOAT3 }
        }
    };

    /* shader and pipeline objects for offscreen-rendering */
    sg_pipeline_desc pip_desc = {
        .shader = sg_make_shader(shapes_shader_desc()),
        .layout = layout,
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
    app.display_cube_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(cube_shader_desc()),
        .layout = layout,
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
    sg_apply_pipeline(app.display_cube_pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = app.cube.vbuf,
        .index_buffer = app.cube.ibuf,
        .fs_images[SLOT_tex] = app.cubemap
    });
    shape_uniforms_t uniforms = {
        .mvp = HMM_MultiplyMat4(view_proj, model),
        .model = model,
        .shape_color = HMM_Vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .light_dir = app.light_dir,
        .eye_pos = HMM_Vec4v(eye_pos, 1.0f)
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_shape_uniforms, &uniforms, sizeof(uniforms));
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
    (void)argc;
    (void)argv;
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
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_shape_uniforms, &uniforms, sizeof(uniforms));
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
