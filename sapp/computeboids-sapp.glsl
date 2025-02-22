// see: https://github.com/austinEng/Project6-Vulkan-Flocking/blob/master/data/shaders/computeparticles/particle.comp

// shared data structures
@block common
struct particle {
    vec2 pos;
    vec2 vel;
};
@end

// compute shader for updating boid positions and velocities
@cs cs
@include_block common

layout(binding=0) readonly buffer cs_ssbo_in { particle prt_in[]; };
layout(binding=1) buffer cs_ssbo_out { particle prt_out[]; };

layout(binding=0) uniform sim_params {
    float dt;
    float rule1_distance;
    float rule2_distance;
    float rule3_distance;
    float rule1_scale;
    float rule2_scale;
    float rule3_scale;
    int num_particles;
};

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;
void main() {
    const uint idx = gl_GlobalInvocationID.x;
    if (idx >= num_particles) {
        return;
    }

    vec2 v_pos = prt_in[idx].pos;
    vec2 v_vel = prt_in[idx].vel;
    vec2 c_mass = vec2(0, 0);
    vec2 c_vel = vec2(0, 0);
    vec2 col_vel = vec2(0, 0);
    int c_mass_count = 0;
    int c_vel_count = 0;
    for (int i = 0; i < num_particles; i++) {
        if (i == idx) {
            continue;
        }
        const vec2 pos = prt_in[i].pos;
        const vec2 vel = prt_in[i].vel;
        const float dist = distance(pos, v_pos);
        if (dist < rule1_distance) {
            c_mass += pos;
            c_mass_count++;
        }
        if (dist < rule2_distance) {
            col_vel -= (pos - v_pos);
        }
        if (dist < rule3_distance) {
            c_vel += vel;
            c_vel_count++;
        }
    }
    if (c_mass_count > 0) {
        c_mass = c_mass / c_mass_count - v_pos;
    }
    if (c_vel_count > 0) {
        c_vel = c_vel / c_vel_count;
    }
    v_vel += c_mass * rule1_scale + col_vel * rule2_scale + c_vel * rule3_scale;

    // clamp velocity for a more pleasing simulation
    v_vel = normalize(v_vel) * clamp(length(v_vel), 0, 0.1);

    // kinematic update
    v_pos += v_vel * dt;
    // wrap around boundary
    if (v_pos.x < -1.0) { v_pos.x = 1.0; }
    else if (v_pos.x > 1.0) { v_pos.x = -1.0; }
    if (v_pos.y < -1.0) { v_pos.y = 1.0; }
    else if (v_pos.y > 1.0) { v_pos.y = -1.0; }

    prt_out[idx].pos = v_pos;
    prt_out[idx].vel = v_vel;
}
@end

@program compute cs

// vertex- and fragment shader for rendering the boids, vertex data is looked
// up from shader constants, per-instance data is coming from a storage buffer
@vs vs
@include_block common

const vec2 verts[3] = { vec2(-0.01, -0.02), vec2(0.01, -0.02), vec2(0.0, 0.02) };

layout(binding=0) readonly buffer vs_ssbo { particle prt[]; };

out vec4 color;

void main() {
    const vec2 v_pos = verts[gl_VertexIndex];
    const vec2 i_pos = prt[gl_InstanceIndex].pos;
    const vec2 i_vel = prt[gl_InstanceIndex].vel;
    const float angle = -atan(i_vel.x, i_vel.y);
    const vec2 pos = vec2(
        (v_pos.x * cos(angle)) - (v_pos.y * sin(angle)),
        (v_pos.x * sin(angle)) + (v_pos.y * cos(angle))
    );
    gl_Position = vec4(pos + i_pos, 0, 1);
    color = vec4(
        1.0 - sin(angle + 1.0) - i_vel.y,
        pos.x * 100.0 - i_vel.y + 0.1,
        i_vel.x + cos(angle + 0.5),
        1.0
    );
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
