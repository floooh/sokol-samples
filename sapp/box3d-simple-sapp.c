//------------------------------------------------------------------------------
//  box3d-simple-sapp.c
//
//  Basic integration with Box3d physics engine.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
#include "box3d/box3d.h"
#include "dbgui/dbgui.h"
#include "util/camera.h"
#include "box3d-simple-sapp.glsl.h"

#define MAX_SHAPES (1024)
#define GROUND_SIZE (200.0f)
#define SPHERE_RADIUS (1.0f)
#define BOX_SIZE (1.5f)
#define SHADOW_MAP_SIZE (2048)
#define USEC_PER_SEC (1000000.0)
#define PHYSICS_TICK_USEC ((1.0 / 250.0) * USEC_PER_SEC)

typedef struct {
    b3BodyId body_id;
    vec3_t color;
    mat44_t transform;
} body_t;

static struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    struct {
        sshape_element_range_t plane;
        sshape_element_range_t sphere;
        sshape_element_range_t box;
    } shapes;
    struct {
        sg_pass pass;
        sg_view tex_view;
        sg_sampler smp;
        sg_pipeline pip;
    } shadow;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
    } display;
    camera_t camera;
    vec3_t light_pos;
    mat44_t light_view_proj;
    mat44_t view_proj;
    struct {
        b3WorldId world;
        b3BodyId ground;
        int64_t tick_error_us;  // current tick error in micro-seconds
        int num_bodies;
        body_t bodies[MAX_SHAPES];
    } physics;
} state;

static void init_gfx(void);
static void init_physics(void);
static void tick_physics(void);
static void add_body(void);
static void cleanup_physics(void);
static void update_matrices(void);
static void shadow_pass_draw_shape(const sshape_element_range_t* shape, mat44_t model);
static void display_pass_draw_shape(const sshape_element_range_t* shape, mat44_t model, vec3_t color);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup();

    // initialize camera helper
    cam_init(&state.camera, &(camera_desc_t){
        .center = vec3(0.0f, 0.0f, 0.0f),
        .latitude = 25.0f,
        .longitude = 225.0f,
        .distance = 50.0f,
        .max_dist = 300.0f,
    });

    init_physics();
    init_gfx();
}

static void frame(void) {
    cam_update(&state.camera, sapp_width(), sapp_height());

    if ((sapp_frame_count() % 30) == 0) {
        add_body();
    }
    tick_physics();
    update_matrices();

    // shadow pass
    sg_begin_pass(&state.shadow.pass);
    sg_apply_pipeline(state.shadow.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
    });
    for (int i = 0; i < state.physics.num_bodies; i++) {
        shadow_pass_draw_shape(&state.shapes.box, state.physics.bodies[i].transform);
    }
    sg_end_pass();

    // display pass
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.ibuf,
        .views[VIEW_shadow_map] = state.shadow.tex_view,
        .samplers[SMP_shadow_sampler] = state.shadow.smp,
    });
    display_pass_draw_shape(&state.shapes.plane, mat44_identity(), vec3(0.5f, 0.5f, 0.5f));
    for (int i = 0; i < state.physics.num_bodies; i++) {
        mat44_t m = state.physics.bodies[i].transform;
        vec3_t c = state.physics.bodies[i].color;
        display_pass_draw_shape(&state.shapes.box, m, c);
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    cleanup_physics();
    __dbgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    if (__dbgui_event_with_retval(ev)) {
        return;
    }
    cam_handle_event(&state.camera, ev);
}

static void update_matrices(void) {
    // calculate matrices for shadow pass
    const mat44_t light_view = mat44_look_at_rh(state.light_pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t light_proj = mat44_ortho_off_center_rh(-50.0f, 50.0f, -50.0f, 50.0f, 1.0f, 400.0f);
    state.light_view_proj = vm_mul(light_view, light_proj);

    // calculate matrices for display pass
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), sapp_widthf()/sapp_heightf(), 0.1f, 500.0f);
    const mat44_t view = mat44_look_at_rh(state.camera.eye_pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    state.view_proj = vm_mul(view, proj);
}

static void shadow_pass_draw_shape(const sshape_element_range_t* shape, mat44_t model) {
    const shadow_vs_params_t vs_params = {
        .mvp = vm_mul(model, state.light_view_proj),
    };
    sg_apply_uniforms(UB_shadow_vs_params, &SG_RANGE(vs_params));
    sg_draw(shape->base_element, shape->num_elements, 1);
}

static void display_pass_draw_shape(const sshape_element_range_t* shape, mat44_t model, vec3_t color) {
    const display_vs_params_t vs_params = {
        .model = model,
        .mvp = vm_mul(model, state.view_proj),
        .light_mvp = vm_mul(model, state.light_view_proj),
        .diff_color = color,
    };
    const display_fs_params_t fs_params = {
        .eye_pos = state.camera.eye_pos,
        .light_dir = vm_normalize(state.light_pos),
    };
    sg_apply_uniforms(UB_display_vs_params, &SG_RANGE(vs_params));
    sg_apply_uniforms(UB_display_fs_params, &SG_RANGE(fs_params));
    sg_draw(shape->base_element, shape->num_elements, 1);
}

static void init_physics(void) {
    b3WorldDef world_def = b3DefaultWorldDef();
    state.physics.world = b3CreateWorld(&world_def);

    b3BodyDef ground_body_def = b3DefaultBodyDef();
    ground_body_def.position = (b3Vec3){ 0.0f, -10.0f, 0.0f };
    state.physics.ground = b3CreateBody(state.physics.world, &ground_body_def);

    const float hs = GROUND_SIZE * 0.5f;
    b3BoxHull ground_box = b3MakeBoxHull(hs, 10.0f, hs);
    b3ShapeDef ground_shape_def = b3DefaultShapeDef();
    b3CreateHullShape(state.physics.ground, &ground_shape_def, &ground_box.base);
}

static void tick_physics(void) {
    double dt_sec = sapp_frame_duration();
    int64_t dt_usec = (int64_t)(dt_sec * USEC_PER_SEC);
    state.physics.tick_error_us += dt_usec;
    int64_t num_steps = state.physics.tick_error_us / PHYSICS_TICK_USEC;
    state.physics.tick_error_us -= num_steps * PHYSICS_TICK_USEC;
    b3World_Step(state.physics.world, (float)dt_sec, (int)num_steps);

    // update moved body transform matrices
    b3BodyEvents events = b3World_GetBodyEvents(state.physics.world);
    for (int i = 0; i < events.moveCount; i++) {
        const b3BodyMoveEvent* ev = &events.moveEvents[i];
        int idx = (int)(intptr_t)ev->userData;
        assert(idx < MAX_SHAPES);
        body_t* body = &state.physics.bodies[idx];
        b3Vec3 p = ev->transform.p;
        b3Quat q = ev->transform.q;
        mat44_t rm = mat44_from_quat(vec4(q.v.x, q.v.y, q.v.z, q.s));
        mat44_t tm = mat44_translation(p.x, p.y, p.z);
        body->transform = vm_mul(rm, tm);
    }
}

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static vec3_t rand_uvec3(void) {
    uint32_t c = xorshift32();
    float x = (float)(c & 255) / 255.0f;
    float y = (float)((c >> 8) & 255) / 255.0;
    float z = (float)((c >> 16) & 255) / 255.0l;
    return (vec3_t){ x, y, z };
}

static vec3_t rand_ivec3(void) {
    vec3_t v = rand_uvec3();
    return vm_mul(vm_sub(v, 0.5f), 2.0f);
}

static void add_body(void) {
    const int idx = state.physics.num_bodies;
    if (idx >= MAX_SHAPES) {
        return;
    }
    const vec3_t pos = vec3(0.0f, 5.0f, 0.0f);
    b3BodyDef body_def = b3DefaultBodyDef();
    body_def.type = b3_dynamicBody;
    body_def.position = (b3Vec3){ pos.x, pos.y, pos.z };
    body_def.userData = (void*)(intptr_t)idx;
    b3BodyId body = b3CreateBody(state.physics.world, &body_def);

    b3BoxHull hull = b3MakeCubeHull(BOX_SIZE * 0.5f);
    b3ShapeDef shape_def = b3DefaultShapeDef();
    shape_def.density = 1.0f;
    shape_def.baseMaterial.friction = 0.3f;
    b3CreateHullShape(body, &shape_def, &hull.base);

    state.physics.bodies[idx].body_id = body;
    state.physics.bodies[idx].color = rand_uvec3();

    // apply linear and angular impulse to get a fountain effect
    vec3_t v = rand_ivec3();
    vec3_t li = vm_mul(vm_normalize(vec3(v.x, v.y + 10.0f, v.z)), 75.0f);
    b3Body_ApplyLinearImpulseToCenter(body, (b3Vec3){ li.x, li.y, li.z }, true);
    v = rand_ivec3();
    vec3_t ai = vm_mul(v, 5.0f);
    b3Body_ApplyAngularImpulse(body, (b3Vec3){ ai.x, ai.y, ai.z }, true);

    state.physics.bodies[idx].transform = mat44_translation(pos.x, pos.y, pos.z);

    state.physics.num_bodies += 1;
}

static void cleanup_physics(void) {
    b3DestroyWorld(state.physics.world);
}

static void init_gfx(void) {
    // fixed light position in world space
    state.light_pos = vec3(50.0f, 100.0f, -75.0f);

    // display pass clear color (blue-ish)
    state.display.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.2f, 0.4f, 0.8f, 1.0f }
        },
    };

    // plane, box and sphere shapes
    uint8_t vertices[SSHAPE_MAX_VERTEX_SIZE * 4096];
    uint16_t indices[4096];
    sshape_state_t shp = {
        .disable = {
            .texcoords = true,
            .colors = true,
        },
        .vertices.buffer = SSHAPE_RANGE(vertices),
        .indices.buffer = SSHAPE_RANGE(indices),
    };
    sshape_build_plane(&shp, &(sshape_plane_t){
        .width = GROUND_SIZE,
        .depth = GROUND_SIZE,
    });
    state.shapes.plane = sshape_element_range(&shp);
    sshape_build_sphere(&shp, &(sshape_sphere_t){
        .radius = SPHERE_RADIUS,
        .slices = 15,
        .stacks = 11,
    });
    state.shapes.sphere = sshape_element_range(&shp);
    sshape_build_box(&shp, &(sshape_box_t){
        .width = BOX_SIZE,
        .height = BOX_SIZE,
        .depth = BOX_SIZE,
    });
    state.shapes.box = sshape_element_range(&shp);

    // shape vertex and index buffer
    const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&shp);
    const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&shp);
    state.vbuf = sg_make_buffer(&vbuf_desc);
    state.ibuf = sg_make_buffer(&ibuf_desc);

    //
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .layout = {
            .buffers[0] = sshape_vertex_buffer_layout_state(&shp),
            .attrs = {
                [ATTR_display_position] = sshape_position_vertex_attr_state(&shp),
                [ATTR_display_normal] = sshape_normal_vertex_attr_state(&shp),
            }
        },
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .label = "display-pipeline"
    });

    // shadow pass resources
    sg_image shadow_map_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = SHADOW_MAP_SIZE,
        .height = SHADOW_MAP_SIZE,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
        .label = "shadow-map-image",
    });
    state.shadow.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = shadow_map_img,
        .label = "shadow-map-texview"
    });
    state.shadow.pass = (sg_pass){
        .action.depth = {
            .load_action = SG_LOADACTION_CLEAR,
            .store_action = SG_STOREACTION_STORE,
            .clear_value = 1.0f,
        },
        .attachments.depth_stencil = sg_make_view(&(sg_view_desc){
            .depth_stencil_attachment.image = shadow_map_img,
            .label = "shadow-map-dsview",
        }),
        .label = "shadow-pass",
    };
    state.shadow.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .compare = SG_COMPAREFUNC_LESS,
        .label = "shadow-map-sampler",
    });
    state.shadow.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(shadow_shader_desc(sg_query_backend())),
        .layout = {
            .buffers[0] = sshape_vertex_buffer_layout_state(&shp),
            .attrs = {
                [ATTR_display_position] = sshape_position_vertex_attr_state(&shp),
            },
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        // render backfaces in shadow pass to prevent shadow acne on front-faces
        .cull_mode = SG_CULLMODE_FRONT,
        .sample_count = 1,
        // 'deactivate' the default color target for 'depth-only-rendering'
        .colors[0].pixel_format = SG_PIXELFORMAT_NONE,
        .label = "shadow-pipeline",
    });
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "box3d-simple-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
