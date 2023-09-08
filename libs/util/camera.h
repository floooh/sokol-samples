#pragma once
/*
    Quick'n'dirty Maya-style camera. Include after HandmadeMath.h
    and sokol_app.h
*/
#include <string.h>
#include <math.h>
#include <assert.h>

#define CAMERA_DEFAULT_MIN_DIST (2.0f)
#define CAMERA_DEFAULT_MAX_DIST (30.0f)
#define CAMERA_DEFAULT_MIN_LAT (-85.0f)
#define CAMERA_DEFAULT_MAX_LAT (85.0f)
#define CAMERA_DEFAULT_DIST (5.0f)
#define CAMERA_DEFAULT_ASPECT (60.0f)
#define CAMERA_DEFAULT_NEARZ (0.01f)
#define CAMERA_DEFAULT_FARZ (100.0f)

typedef struct {
    float min_dist;
    float max_dist;
    float min_lat;
    float max_lat;
    float distance;
    float latitude;
    float longitude;
    float aspect;
    float nearz;
    float farz;
    hmm_vec3 center;
} camera_desc_t;

typedef struct {
    float min_dist;
    float max_dist;
    float min_lat;
    float max_lat;
    float distance;
    float latitude;
    float longitude;
    float aspect;
    float nearz;
    float farz;
    hmm_vec3 center;
    hmm_vec3 eye_pos;
    hmm_mat4 view;
    hmm_mat4 proj;
    hmm_mat4 view_proj;
} camera_t;

static float _cam_def(float val, float def) {
    return ((val == 0.0f) ? def:val);
}

/* initialize to default parameters */
static void cam_init(camera_t* cam, const camera_desc_t* desc) {
    assert(cam && desc);
    memset(cam, 0, sizeof(camera_t));
    cam->min_dist = _cam_def(desc->min_dist, CAMERA_DEFAULT_MIN_DIST);
    cam->max_dist = _cam_def(desc->max_dist, CAMERA_DEFAULT_MAX_DIST);
    cam->min_lat = _cam_def(desc->min_lat, CAMERA_DEFAULT_MIN_LAT);
    cam->max_lat = _cam_def(desc->max_lat, CAMERA_DEFAULT_MAX_LAT);
    cam->distance = _cam_def(desc->distance, CAMERA_DEFAULT_DIST);
    cam->center = desc->center;
    cam->latitude = desc->latitude;
    cam->longitude = desc->longitude;
    cam->aspect = _cam_def(desc->aspect, CAMERA_DEFAULT_ASPECT);
    cam->nearz = _cam_def(desc->nearz, CAMERA_DEFAULT_NEARZ);
    cam->farz = _cam_def(desc->farz, CAMERA_DEFAULT_FARZ);
}

/* feed mouse movement */
static void cam_orbit(camera_t* cam, float dx, float dy) {
    assert(cam);
    cam->longitude -= dx;
    if (cam->longitude < 0.0f) {
        cam->longitude += 360.0f;
    }
    if (cam->longitude > 360.0f) {
        cam->longitude -= 360.0f;
    }
    cam->latitude = HMM_Clamp(cam->min_lat, cam->latitude + dy, cam->max_lat);
}

/* feed zoom (mouse wheel) input */
static void cam_zoom(camera_t* cam, float d) {
    assert(cam);
    cam->distance = HMM_Clamp(cam->min_dist, cam->distance + d, cam->max_dist);
}

static hmm_vec3 _cam_euclidean(float latitude, float longitude) {
    const float lat = HMM_ToRadians(latitude);
    const float lng = HMM_ToRadians(longitude);
    return HMM_Vec3(cosf(lat) * sinf(lng), sinf(lat), cosf(lat) * cosf(lng));
}

/* update the view, proj and view_proj matrix */
static void cam_update(camera_t* cam, int fb_width, int fb_height) {
    assert(cam);
    assert((fb_width > 0) && (fb_height > 0));
    const float w = (float) fb_width;
    const float h = (float) fb_height;
    cam->eye_pos = HMM_AddVec3(cam->center, HMM_MultiplyVec3f(_cam_euclidean(cam->latitude, cam->longitude), cam->distance));
    cam->view = HMM_LookAt(cam->eye_pos, cam->center, HMM_Vec3(0.0f, 1.0f, 0.0f));
    cam->proj = HMM_Perspective(cam->aspect, w/h, cam->nearz, cam->farz);
    cam->view_proj = HMM_MultiplyMat4(cam->proj, cam->view);
}

/* handle sokol-app input events */
static void cam_handle_event(camera_t* cam, const sapp_event* ev) {
    assert(cam);
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                sapp_lock_mouse(true);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                sapp_lock_mouse(false);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            cam_zoom(cam, ev->scroll_y * 0.5f);
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            if (sapp_mouse_locked()) {
                cam_orbit(cam, ev->mouse_dx * 0.25f, ev->mouse_dy * 0.25f);
            }
            break;
        default:
            break;
    }
}
