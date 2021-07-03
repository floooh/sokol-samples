#pragma once
/*
    A quick'n'dirty C-API wrapper for some ozz-animation features.
*/
#include <stdint.h>
#include <stdbool.h>
#include "sokol_gfx.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef void* ozz_t;
typedef void* ozz_instance_t;

typedef struct {
    float position[3];
    uint32_t normal;
    uint32_t joint_indices;
    uint32_t joint_weights;
} ozz_vertex_t;

typedef struct {
    int max_palette_joints;
    int max_instances;
} ozz_desc_t;

void ozz_setup(const ozz_desc_t* desc);
void ozz_shutdown(void);
sg_image ozz_joint_texture(void);
ozz_instance_t* ozz_create_instance(int index);
void ozz_destroy_instance(ozz_instance_t* ozz);
sg_buffer ozz_vertex_buffer(ozz_instance_t* ozz);
sg_buffer ozz_index_buffer(ozz_instance_t* ozz);
bool ozz_all_loaded(ozz_instance_t* ozz);
bool ozz_load_failed(ozz_instance_t* ozz);
void ozz_load_skeleton(ozz_instance_t* ozz, const void* data, size_t num_bytes);
void ozz_load_animation(ozz_instance_t* ozz, const void* data, size_t num_bytes);
void ozz_load_mesh(ozz_instance_t* ozz, const void* data, size_t num_bytes);
void ozz_set_load_failed(ozz_instance_t* ozz);
void ozz_update_instance(ozz_instance_t* ozz, double seconds);
void ozz_update_joint_texture(void);
float ozz_joint_texture_pixel_width(void);
float ozz_joint_texture_u(ozz_instance_t* ozz);
float ozz_joint_texture_v(ozz_instance_t* ozz);
int ozz_num_triangle_indices(ozz_instance_t* ozz);

#if defined(__cplusplus)
} // extern "C"
#endif
