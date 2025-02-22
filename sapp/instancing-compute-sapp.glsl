@ctype mat4 hmm_mat4
@ctype vec3 hmm_vec3
@ctype vec4 hmm_vec4

// shared data structures
@block common
struct particle {
    vec4 pos;
    vec4 vel;
};
@end

// compute-shader for initializing pseudo-random particle velocities
@cs cs_init
@include_block common

layout(binding=0) buffer cs_ssbo {
    particle prt[];
};

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

uint xorshift32(uint x) {
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint x = xorshift32(0x12345678 ^ idx);
    uint y = xorshift32(x);
    uint z = xorshift32(y);
    prt[idx].vel = vec4(
        (float(x & 0x7FFF) / 0x7FFF) - 0.5f,
        (float(y & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f,
        (float(z & 0x7FFF) / 0x7FFF) - 0.5f,
        0);
}
@end
@program init cs_init

// compute-shader for updating particle positions
@cs cs_update
@include_block common

layout(binding=0) uniform cs_params {
    float dt;
    int num_particles;
};

layout(binding=0) buffer cs_ssbo {
    particle prt[];
};

layout(local_size_x=64, locaL_size_y=1, local_size_z=1) in;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= num_particles) {
        return;
    }
    vec4 pos = prt[idx].pos;
    vec4 vel = prt[idx].vel;
    vel.y -= 1.0 * dt;
    pos += vel * dt;
    if (pos.y < -2.0) {
        pos.y = -1.8;
        vel *= vec4(0.8, -0.8, 0.8, 0.0);
    }
    prt[idx].pos = pos;
    prt[idx].vel = vel;
}
@end
@program update cs_update

// vertex- and fragment-shader for rendering the particles
@vs vs
@include_block common

layout(binding=0) uniform vs_params {
    mat4 mvp;
};

layout(binding=0) readonly buffer vs_ssbo {
    particle prt[];
};

in vec3 pos;
in vec4 color0;

out vec4 color;

void main() {
    vec3 inst_pos = prt[gl_InstanceIndex].pos.xyz;
    vec4 pos = vec4(pos +  inst_pos, 1.0);
    gl_Position = mvp * pos;
    color = color0;
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;
void main() {
    frag_color = color;
}
@end

@program display vs fs
