@ctype mat4 mat44_t

@const int WORKGROUP_SIZE 64
@const int NUM_CUBE_INDICES 36

// shared data structures
@block common
const float TAU = 6.28318530718;
const float SIZE_SCALE = 0.005;
const float ROT_SCALE = 0.5;
const float VEL_SCALE = 0.1;

// complete item state
struct item {
    vec4 pos;   // xyz: current position (-1 .. +1)
    vec4 rot;   // xyz: normalized rotation axis, w: current angle in 'turns' (0..1)
    vec4 vel;   // xyz: pos velocity, w: angular velocity
};

// an index of a visible item into the item state buffer
struct visible_item {
    uint item_index;
};

// draw-indirect layout for indexed rendering (same as sg_draw_indirect_indexed_args)
struct draw_args {
    uint num_elements;
    uint num_instances;
    uint base_element;
    int base_vertex;
    uint base_instance;
};
@end

// compute shader to initialize position, velocity,
// rotation axis, current angle and angular velocity
// to pseudo-random values
@cs cs_init
@include_block common
layout(binding=0) buffer cs_items { item items[]; };
layout(local_size_x=WORKGROUP_SIZE, local_size_y=1, local_size_z=1) in;

uint xorshift32(uint x) {
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

uvec4 xorshift32v(uvec4 v) {
    uvec4 res = uvec4(0);
    res.x = xorshift32(v.w);
    res.y = xorshift32(res.x);
    res.z = xorshift32(res.y);
    res.w = xorshift32(res.z);
    return res;
}

// convert uint random number into float in range 0..1
float to_flt(uint val) {
    return float(val & 0x7FFFu) / 0x7FFFu;
}

vec4 to_pos(uvec4 p_in) {
    vec3 p = vec3(to_flt(p_in.x), to_flt(p_in.y), to_flt(p_in.z));
    return vec4(p * 2.0 - 1.0, 0.0);
}

vec4 to_vel(uvec4 v_in) {
    vec3 v = vec3(to_flt(v_in.x), to_flt(v_in.y), to_flt(v_in.z));
    return vec4(normalize(v * 2.0 - 1.0) * VEL_SCALE, (to_flt(v_in.w) * 2.0 - 1.0) * ROT_SCALE);
}

vec4 to_axis_angle(uvec4 a_in) {
    vec3 axis = normalize(vec3(to_flt(a_in.x), to_flt(a_in.y), to_flt(a_in.z)) * 2.0 - 1.0);
    return vec4(axis, to_flt(a_in.w));
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uvec4 p = xorshift32v(uvec4(idx + 1u) * 0x9E3779B9u);
    uvec4 v = xorshift32v(p);
    uvec4 r = xorshift32v(v);
    items[idx].pos = to_pos(p);
    items[idx].rot = to_axis_angle(r);
    items[idx].vel = to_vel(v);
}
@end
@program init cs_init

// a 'reset shader' to reset the draw-indirect args at start of frame
@cs cs_reset
@include_block common
layout(binding=1) buffer cs_indirect { draw_args args[]; };
layout(local_size_x=32, local_size_y=1, local_size_z=1) in;

void main() {
    if (gl_GlobalInvocationID.x == 0) {
        args[0].num_elements = NUM_CUBE_INDICES;
        args[0].num_instances = 0;
        args[0].base_element = 0;
        args[0].base_vertex = 0;
        args[0].base_instance = 0;
    }
}
@end
@program reset cs_reset

// a combined update and culling shader: first update item position
// and rotation to tumble around in a cube volume, then cull against
// a 'gyroid' surface. Visible items are added as index into the
// 'all-items' buffer, and the instance-count in the indirect-buffer
// is bumped.
@cs cs_upd_cull
@include_block common

layout(binding=0) uniform cs_upd_cull_params {
    float dt;
    float scale;
    float width;
    float phase;
};

layout(binding=0) buffer cs_items { item items[]; };
layout(binding=1) buffer cs_indirect { draw_args args[]; };
layout(binding=2) buffer cs_visible { visible_item visible[]; };
layout(local_size_x=WORKGROUP_SIZE, local_size_y=1, local_size_z=1) in;

void main() {
    // read initial position, rotation and linear+angular velocity
    uint idx = gl_GlobalInvocationID.x;
    vec4 pos = items[idx].pos;
    vec4 rot = items[idx].rot;
    vec4 vel = items[idx].vel;

    // update position and rotation
    pos.xyz = pos.xyz + vel.xyz * dt;
    rot.w = fract(rot.w + vel.w * dt);

    // bounce against cube volume [-1 .. +1]
    bvec3 outside = greaterThan(abs(pos.xyz), vec3(1.0));
    vel.xyz = mix(vel.xyz, -vel.xyz, outside);
    pos.xyz = clamp(pos.xyz, -1.0, 1.0);

    // write back position and rotation
    items[idx].pos = pos;
    items[idx].rot = rot;
    items[idx].vel = vel;

    // items close to the gyroid surface remain visible
    vec3 p = pos.xyz * scale + phase;
    float d = dot(sin(p), cos(p.yzx));
    if (abs(d) < width) {
        // item is visible, bump indirect num_instances arg
        // and write index into items buffer
        uint slot = atomicAdd(args[0].num_instances, 1u);
        visible[slot].item_index = idx;
    }
}
@end
@program upd_cull cs_upd_cull

// a shader for rendering the cubes in the display-pass
@vs vs
@include_block common
layout(binding=0) uniform vs_params {
    mat4 view_proj;
};

layout(binding=0) readonly buffer vs_items { item items[]; };
layout(binding=1) readonly buffer vs_visible { visible_item visible[]; };

in vec4 in_pos;
in float in_intensity;

out vec4 color;

// rotate p around normalized axis k by angle a (Rodrigues)
vec3 rotate(vec3 p, vec3 k, float a) {
    float c = cos(a);
    float s = sin(a);
    return p * c + cross(k, p) * s + k * dot(k, p) * (1.0 - c);
}

vec3 rainbow(float hue) {
    return clamp(abs(fract(hue + vec3(0.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
}

void main() {
    uint item_idx = visible[gl_InstanceIndex].item_index;
    vec4 item_pos = items[item_idx].pos;
    vec4 item_rot = items[item_idx].rot;
    vec3 world_pos = item_pos.xyz + rotate(in_pos.xyz * SIZE_SCALE, item_rot.xyz, item_rot.w * TAU);
    gl_Position = view_proj * vec4(world_pos, 1.0);
    // compute an 'interesting' color
    float hue = length(item_pos.xyz);
    color = vec4(rainbow(hue) * in_intensity, 1.0);
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = vec4(color.rgb, 1);
}
@end

@program display vs fs
