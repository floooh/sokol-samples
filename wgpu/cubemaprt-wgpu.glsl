//------------------------------------------------------------------------------
//  shaders for cubemaprt-wgpu sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4
@ctype vec4 hmm_vec4

// same vertex shader for offscreen- and default-pass
@vs vs
uniform shape_uniforms {
    mat4 mvp;           // model-view-projection matrix
    mat4 model;         // model matrix
    vec4 shape_color;
    vec4 light_dir;     // light-direction in world space
    vec4 eye_pos;       // eye-pos in world space
};

in vec4 pos;
in vec3 norm;

out vec3 world_position;
out vec3 world_normal;
out vec3 world_eyepos;
out vec3 world_lightdir;
out vec4 color;

void main() {
    gl_Position = mvp * pos;
    world_position = vec4(model * pos).xyz;
    world_normal = vec4(model * vec4(norm, 0.0)).xyz;
    world_eyepos = eye_pos.xyz;
    world_lightdir = light_dir.xyz;
    color = shape_color;
}
@end

// shared code for fragment shaders
@block lighting
vec3 light(vec3 base_color, vec3 eye_vec, vec3 normal, vec3 light_vec) {
    float ambient = 0.25;
    float n_dot_l = max(dot(normal, light_vec), 0.0);
    float diff = n_dot_l + ambient;
    float spec_power = 16.0;
    vec3 r = reflect(-light_vec, normal);
    float r_dot_v = max(dot(r, eye_vec), 0.0);
    float spec = pow(r_dot_v, spec_power) * n_dot_l;
    return base_color * (diff+ambient) + vec3(spec,spec,spec);
}
@end

@block fs_inputs
in vec3 world_position;
in vec3 world_normal;
in vec3 world_eyepos;
in vec3 world_lightdir;
in vec4 color;
@end

// fragment shader for shaped rendering, both offscreen and display
@fs fs_shapes
@include_block lighting
@include_block fs_inputs

out vec4 frag_color;

void main() {
    vec3 eye_vec = normalize(world_eyepos - world_position);
    vec3 nrm = normalize(world_normal);
    vec3 light_dir = normalize(world_lightdir);
    frag_color = vec4(light(color.xyz, eye_vec, nrm, light_dir), 1.0);
}
@end

// fragment shader for the central cubemapped cube
@fs fs_cube
@include_block lighting
@include_block fs_inputs

uniform samplerCube tex;
out vec4 frag_color;

void main() {
    vec3 eye_vec = normalize(world_eyepos - world_position);
    vec3 nrm = normalize(world_normal);
    vec3 light_dir = normalize(world_lightdir);
    vec3 refl_vec = normalize(world_position);
    vec3 refl_color = texture(tex, refl_vec).xyz;
    frag_color = vec4(light(refl_color * color.xyz, eye_vec, nrm, light_dir), 1.0);
}
@end

@program shapes vs fs_shapes
@program cube vs fs_cube
