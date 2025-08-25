//------------------------------------------------------------------------------
//  sharedbindings-sapp.c
//
//  Test/demonstrate how to share a common sg_bindings struct between
//  different shaders using explicit bindslots, and also test that
//  gaps between bind slots work.
//------------------------------------------------------------------------------
#define VECMATH_GENERICS
#include "vecmath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "shared-bindings-sapp.glsl.h"

#define RED_CUBE    (0)
#define GREEN_CUBE  (1)
#define BLUE_CUBE   (2)
#define NUM_CUBES   (3)

static struct {
    float rx, ry;
    sg_bindings bind;
    sg_pipeline pip[NUM_CUBES];
    sg_pass_action pass_action;
} state;

typedef struct {
    float x, y, z;
    uint32_t color;
    int16_t u, v;
} vertex_t;

static mat44_t compute_mvp(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // pass action for clearing the framebuffer
    state.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    // a cube vertex buffer
    vertex_t vertices[] = {
        // pos                  color       uvs
        { -1.0f, -1.0f, -1.0f,  0xFFFFFFFF,     0,     0 },
        {  1.0f, -1.0f, -1.0f,  0xFFFFFFFF, 32767,     0 },
        {  1.0f,  1.0f, -1.0f,  0xFFFFFFFF, 32767, 32767 },
        { -1.0f,  1.0f, -1.0f,  0xFFFFFFFF,     0, 32767 },

        { -1.0f, -1.0f,  1.0f,  0xFFDDDDDD,     0,     0 },
        {  1.0f, -1.0f,  1.0f,  0xFFDDDDDD, 32767,     0 },
        {  1.0f,  1.0f,  1.0f,  0xFFDDDDDD, 32767, 32767 },
        { -1.0f,  1.0f,  1.0f,  0xFFDDDDDD,     0, 32767 },

        { -1.0f, -1.0f, -1.0f,  0xFFBBBBBB,     0,     0 },
        { -1.0f,  1.0f, -1.0f,  0xFFBBBBBB, 32767,     0 },
        { -1.0f,  1.0f,  1.0f,  0xFFBBBBBB, 32767, 32767 },
        { -1.0f, -1.0f,  1.0f,  0xFFBBBBBB,     0, 32767 },

        {  1.0f, -1.0f, -1.0f,  0xFF999999,     0,     0 },
        {  1.0f,  1.0f, -1.0f,  0xFF999999, 32767,     0 },
        {  1.0f,  1.0f,  1.0f,  0xFF999999, 32767, 32767 },
        {  1.0f, -1.0f,  1.0f,  0xFF999999,     0, 32767 },

        { -1.0f, -1.0f, -1.0f,  0xFF777777,     0,     0 },
        { -1.0f, -1.0f,  1.0f,  0xFF777777, 32767,     0 },
        {  1.0f, -1.0f,  1.0f,  0xFF777777, 32767, 32767 },
        {  1.0f, -1.0f, -1.0f,  0xFF777777,     0, 32767 },

        { -1.0f,  1.0f, -1.0f,  0xFF555555,     0,     0 },
        { -1.0f,  1.0f,  1.0f,  0xFF555555, 32767,     0 },
        {  1.0f,  1.0f,  1.0f,  0xFF555555, 32767, 32767 },
        {  1.0f,  1.0f, -1.0f,  0xFF555555,     0, 32767 },
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });

    // create an index buffer for the cube
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    // create 3 textures, shaders and pipeline objects
    for (int i = 0; i < NUM_CUBES; i++) {
        int view_slot, smp_slot;
        uint32_t color;
        const char* image_label;
        const char* view_label;
        const char* smp_label;
        const char* pip_label;
        sg_shader shd;
        switch (i) {
            case RED_CUBE:
                view_slot = VIEW_tex_red;
                smp_slot = SMP_smp_red;
                color = 0xFF0000FF;
                image_label = "red-image";
                view_label = "red-texture-view";
                smp_label = "red-sampler";
                pip_label = "red-pipeline";
                shd = sg_make_shader(red_shader_desc(sg_query_backend()));
                break;
            case GREEN_CUBE:
                view_slot = VIEW_tex_green;
                smp_slot = SMP_smp_green;
                color = 0xFF00FF00;
                image_label = "green-image";
                view_label = "green-texture-view";
                smp_label = "green-sampler";
                pip_label = "green-pipeline";
                shd = sg_make_shader(green_shader_desc(sg_query_backend()));
                break;
            default:
                view_slot = VIEW_tex_blue;
                smp_slot = SMP_smp_blue;
                color = 0xFFFF0000;
                image_label = "blue-image";
                view_label = "blue-texture-view";
                smp_label = "blue-sampler";
                pip_label = "blue-pipeline";
                shd = sg_make_shader(blue_shader_desc(sg_query_backend()));
                break;
        }
        uint32_t pixels[4][4];
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                // make a checkerboard
                pixels[y][x] = ((x ^ y) & 1) ? color : 0xFF000000;
            }
        }
        state.bind.views[view_slot] = sg_make_view(&(sg_view_desc){
            .texture = {
                .image = sg_make_image(&(sg_image_desc){
                    .width = 4,
                    .height = 4,
                    .data.subimage[0][0] = SG_RANGE(pixels),
                    .label = image_label,
                }),
            },
            .label = view_label,
        });
        state.bind.samplers[smp_slot] = sg_make_sampler(&(sg_sampler_desc){
            .label = smp_label,
        });
        state.pip[i] = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = shd,
            .layout.attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_UBYTE4N,
                [2].format = SG_VERTEXFORMAT_SHORT2N,
            },
            .index_type = SG_INDEXTYPE_UINT16,
            .cull_mode = SG_CULLMODE_BACK,
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
            .label = pip_label,
        });
    }
}

static void frame(void) {
    const float dt = (float)(sapp_frame_duration() * 60.0);
    const float dw = sapp_widthf();
    const float dh = sapp_heightf();

    state.rx += 1.0f * dt; state.ry += 2.0f * dt;

    const vs_params_t vs_params = { .mvp = compute_mvp() };

    const float vpw = dw * 0.333f;
    const float vph = vpw;
    const float vpy = dh * 0.5f - vph * 0.5f;

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    for (int i = 0; i < 3; i++) {
        const float vpx = dw * 0.5f - 1.5f * vpw + i * vpw;
        sg_apply_viewportf(vpx, vpy, vpw, vph, true);
        sg_apply_pipeline(state.pip[i]);
        sg_apply_bindings(&state.bind);
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
        sg_draw(0, 36, 1);
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

static mat44_t compute_mvp(void) {
    const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), 1.0f, 0.01f, 10.0f);
    const mat44_t view = mat44_look_at_rh(vec3(0.0f, 0.0f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    const mat44_t rxm = mat44_rotation_x(vm_radians(state.rx));
    const mat44_t rym = mat44_rotation_y(vm_radians(state.ry));
    const mat44_t model = vm_mul(rym, rxm);
    return vm_mul(model, vm_mul(view, proj));
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
        .sample_count = 4,
        .window_title = "shared-bindings-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
