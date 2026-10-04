@ctype mat4 mat44_t

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
    mat4 model;
};

layout(location=0) in vec4 in_pos;
layout(location=1) in vec3 in_normal;
layout(location=2) in vec2 in_uv;
layout(location=3) in vec4 in_color;

out vec3 normal;
out vec2 uv;
out vec4 color;

void main() {
    gl_Position = mvp * in_pos;
    normal = (model * vec4(in_normal, 0)).xyz;
    uv = in_uv * 10;
    color = in_color;
}
@end

@fs fs
in vec3 normal;
in vec2 uv;
in vec4 color;

out vec4 frag_color;

vec3 gamma(vec3 c) {
    return pow(c, vec3(1.0/2.2));
}

void main() {
    const vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
    const float ambient = 0.1;
    const float n_dot_l = max(dot(normalize(normal), light_dir), 0.0);
    // simple checkboard pattern from uv
    float cb = max(mod(floor(uv.x) + floor(uv.y), 2.0), 0.15);
    const vec3 rgb = vec3(cb) * color.rgb * (ambient + n_dot_l);
    frag_color = vec4(gamma(rgb), 1);
}
@end

@program shape vs fs
