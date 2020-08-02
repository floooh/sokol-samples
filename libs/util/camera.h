#pragma once
/*
    Quick'n'dirty Maya-style camera. Include after HandmadeMath.h
*/
#include <string.h>
#include <math.h>

typedef struct {
    float min_dist;
    float max_dist;
    float min_lat;
    float max_lat;
    float dist;
    float aspect;
    hmm_vec2 polar;
    hmm_vec3 center;
    hmm_vec3 eye_pos;
    hmm_mat4 view;
    hmm_mat4 proj;
    hmm_mat4 view_proj;
} camera_t;

/* initialize to default parameters */
static void cam_init(camera_t* cam) {
    memset(cam, 0, sizeof(camera_t));
    cam->min_dist = 2.0f;
    cam->max_dist = 30.0f;
    cam->min_lat = -85.0f;
    cam->max_lat = 85.0f;
    cam->dist = 5.0f;
    cam->aspect = 60.0f;
    cam->polar = HMM_Vec2(-10.0f, 45.0f);
}

/* feed mouse movement */
static void cam_orbit(camera_t* cam, float dx, float dy) {
    cam->polar.Y -= dx;
    cam->polar.X = HMM_Clamp(cam->min_lat, cam->polar.X + dy, cam->max_lat);
}

/* feed zoom (mouse wheel) input */
static void cam_zoom(camera_t* cam, float d) {
    cam->dist = HMM_Clamp(cam->min_dist, cam->dist + d, cam->max_dist);
}

/* math helper functions */
#define _cam_mul(v, s) HMM_MultiplyVec3f(v, s)
#define _cam_add(v0, v1) HMM_AddVec3(v0, v1)

static hmm_vec3 _cam_euclidean(hmm_vec2 polar) {
    const float lat = HMM_ToRadians(polar.X);
    const float lng = HMM_ToRadians(polar.Y);
    return HMM_Vec3(cosf(lat) * sinf(lng), sinf(lat), cosf(lat) * cosf(lng));
}

/* update the view, proj and view_proj matrix */
static void cam_update(camera_t* cam, int fb_width, int fb_height) {
    const float w = (float) fb_width;
    const float h = (float) fb_height;
    cam->eye_pos = _cam_add(cam->center, _cam_mul(_cam_euclidean(cam->polar), cam->dist));
    cam->view = HMM_LookAt(cam->eye_pos, cam->center, HMM_Vec3(0.0f, 1.0f, 0.0f));
    cam->proj = HMM_Perspective(cam->aspect, w/h, 0.01f, 100.0f);
    cam->view_proj = HMM_MultiplyMat4(cam->proj, cam->view);
}
