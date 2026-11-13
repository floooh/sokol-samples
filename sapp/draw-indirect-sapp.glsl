@ctype mat4 mat44_t

// shared data structures
@block common
const float TAU = 6.28318530718;

struct item_props {
    vec4 pos;   // xyz: current position (-1 .. +1)
    vec4 rot;   // xyz: normalized rotation axis, w: current angle in 'turns' (0..1)
    vec4 vel;   // xyz: pos velocity, w: angular velocity
};
@end

// compute shader to initialize position, velocity,
// rotation axis, current angle and angular velocity
// to pseudo-random values
@cs init
@include_block common
layout(binding=0) buffer cs_ssbo { item_props items[]; };
layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

uint xorshift32(uint x) {
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

// convert uint random number into float in range 0..1
float to_flt(uint val) {
    return float(val & 0x7FFFu) / 0x7FFFu;
}

vec4 to_pos(uint x, uint y, uint z) {
    vec3 p = vec3(to_flt(x), to_flt(y), to_flt(z));
    return vec4(p * 2.0 - 1.0, 0.0);
}

vec4 to_vel(uint x, uint y, uint z, uint a) {
    vec3 v = vec3(to_flt(x), to_flt(y), to_flt(z));
    return vec4(normalize(v * 2.0 - 1.0), to_flt(a));
}

vec4 to_axis_angle(uint x, uint y, uint z, uint a) {
    vec3 axis = normalize(vec3(to_flt(x), to_flt(y), to_flt(z)) * 2.0 - 1.0);
    return vec4(axis, to_flt(a));
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint px = xorshift32((idx + 1u) * 0x9E3779B9u);
    uint py = xorshift32(px);
    uint pz = xorshift32(py);
    uint vx = xorshift32(pz);
    uint vy = xorshift32(vx);
    uint vz = xorshift32(vy);
    uint va = xorshift32(vz);
    uint rx = xorshift32(va);
    uint ry = xorshift32(rx);
    uint rz = xorshift32(ry);
    uint ra = xorshift32(rz);
    items[idx].pos = to_pos(px, py, pz);
    items[idx].rot = to_axis_angle(rx, ry, rz, ra);
    items[idx].vel = to_vel(vx, vy, vz, va);
}
@end
@program cs_init init

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
};

in vec4 position;
in vec4 color0;

out vec4 color;

void main() {
    gl_Position = mvp * position;
    color = color0;
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = vec4(color.rgb, 1);
}
@end

@program cube vs fs
