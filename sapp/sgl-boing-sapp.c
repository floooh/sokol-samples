//------------------------------------------------------------------------------
//  sgl-boing-sapp.c
//
//  Amiga-style bouncing ball demo via sokol_gl.h.
//
//  Ported from Nim sample by @aguspiza (https://github.com/floooh/sokol-nim/pull/40)
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "dbgui/dbgui.h"
#define _USE_MATH_DEFINES
#include <math.h>

static struct {
    sg_pass_action pass_action;
    float ball_x, ball_y;
    float ball_vx, ball_vy;
    float ball_radius;
    float ball_rotz, ball_rotx;
} state;

static void draw_ball(float x, float y, float r, float rotz, float rotx);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());
    sgl_setup(&(sgl_desc_t){ .logger.func = slog_func });

    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.5f, 0.5f, 0.5f, 1.0f },
        }
    };
    state.ball_radius = 70.0f;
    state.ball_x = state.ball_radius + 20.0f;
    state.ball_y = sapp_heightf() - state.ball_radius - 20.0f;
    state.ball_vx = 300.0f;
    state.ball_vy = -600.0f;
}

static void frame(void) {
    const float dt = (float)sapp_frame_duration();
    const float gravity = 800.0f;

    // apply gravity
    state.ball_vy += gravity * dt;

    // update position
    state.ball_x += state.ball_vx * dt;
    state.ball_y += state.ball_vy * dt;

    // bounce off floor
    const float floor_y = sapp_heightf() - state.ball_radius;
    if (state.ball_y > floor_y) {
        state.ball_y = floor_y;
        state.ball_vy = -600.0f;    // fixed upward velocity = constant bounce height
    }

    // bounce off walls
    if (state.ball_x < state.ball_radius) {
        state.ball_x = state.ball_radius;
        state.ball_vx = -state.ball_vx;
    }
    if (state.ball_x > sapp_widthf() - state.ball_radius) {
        state.ball_x = sapp_widthf() - state.ball_radius;
        state.ball_vx = -state.ball_vx;
    }

    // update rotation based on movement
    state.ball_rotz += state.ball_vx * dt * 0.3f;    // rotate with horizontal movement
    state.ball_rotx += state.ball_vy * dt * 0.2f;    // y-axis spin with vertical movement

    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, sapp_width(), sapp_heightf(), 0.0f, -100.0f, +100.0f);
    sgl_matrix_mode_modelview();
    draw_ball(state.ball_x, state.ball_y, state.ball_radius, state.ball_rotz, state.ball_rotx);

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sgl_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();

}

static void cleanup(void) {
    __dbgui_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

static void draw_ball(float x, float y, float r, float rotz, float rotx) {
    const int bands = 12;
    const int segs = 12;
    sgl_push_matrix();
    sgl_translate(x, y, 0.0f);
    sgl_rotate(sgl_rad(rotz), 0.0f, 0.0f, 1.0f);
    sgl_rotate(sgl_rad(rotx), 1.0f, 0.0f, 0.0f);
    sgl_begin_quads();
    for (int lat = 0; lat < bands; lat++) {
        const float latf = (float)lat;
        const float t1 = M_PI * (latf / (float)bands) - M_PI * 0.5f;
        const float t2 = M_PI * ((latf + 1.0f) / (float)bands) - M_PI * 0.5f;
        const float sint1 = sinf(t1);
        const float cost1 = cosf(t1);
        const float sint2 = sinf(t2);
        const float cost2 = cosf(t2);
        for (int lon = 0; lon < segs; lon++) {
            const float lonf = (float)lon;
            const float p1 = 2.0f * M_PI * (lonf / (float)segs);
            const float p2 = 2.0f * M_PI * ((lonf + 1.0f) / (float)segs);
            const bool is_red = 0 != ((lat + lon) & 1);
            const float sinp1 = sinf(p1);
            const float cosp1 = cosf(p1);
            const float sinp2 = sinf(p2);
            const float cosp2 = cosf(p2);
            if (is_red) {
                sgl_c3f(0.9f, 0.1f, 0.1f);
            } else {
                sgl_c3f(1.0f, 1.0f, 1.0f);
            }
            sgl_v3f(r * cost1 * cosp1, r * sint1, r * cost1 * sinp1);
            sgl_v3f(r * cost1 * cosp2, r * sint1, r * cost1 * sinp2);
            sgl_v3f(r * cost2 * cosp2, r * sint2, r * cost2 * sinp2);
            sgl_v3f(r * cost2 * cosp1, r * sint2, r * cost2 * sinp1);
        }
    }
    sgl_end();
    sgl_pop_matrix();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .window_title = "sgl-boing-sapp.c",
        .icon.sokol_default = true,
    };
}
