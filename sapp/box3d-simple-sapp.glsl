@ctype mat4 mat44_t

@block util
vec4 gamma(vec4 c) {
    float p = 1.0/2.2;
    return vec4(pow(c.xyz, vec3(p)), c.w);
}
@end

@vs shape_vs
layout(binding=0) uniform shape_vs_params {
    mat4 mvp;
    mat4 model;
    mat4 light_mvp;
    vec3 diff_color;
};

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;

out vec3 color;
//out vec4 light_proj_pos;    // light space position (for shadow mapping)
out vec3 P; // world space position
out vec3 N; // world space normal

void main() {
    gl_Position = mvp * position;
    // light_proj_pos = light_mvp * position;
    P = (model * position).xyz;
    N = (model * vec4(normal, 0)).xyz;
    color = diff_color;
}
@end

@vs shape_fs
@include_block util
layout(binding=1) uniform shape_fs_params {
    vec3 light_dir;
    vec3 eye_pos;
};

in vec3 color;
//in vec4 light_proj_pos;
in vec3 P;
in vec3 N;

out vec4 frag_color;

void main() {
    float spec_power = 16.0;
    float ambient_intensity = 0.25;

    // diffuse lighting
    vec3 l = light_dir;
    vec3 n = normalize(N);
    float n_dot_l = dot(n, l);
    if (n_dot_l > 0) {
        // FIXME: shadow mapping
        float s = 1.0;
        float diff_intensity = max(n_dot_l * s, 0);
        vec3 v = normalize(eye_pos - P);
        vec3 r = reflect(-l, n);
        float r_dot_v = max(dot(r, v), 0.0);
        float spec_intensity = pow(r_dot_v, spec_power) * n_dot_l * s;

        frag_color = vec4(vec3(spec_intensity) + (diff_intensity + ambient_intensity) * color, 1);
    } else {
        // FIXME: more interesting ambient color
        frag_color = vec4(color * ambient_intensity, 1);
    }
    frag_color = gamma(frag_color);
}
@end
