@ctype mat4 hmm_mat4
@ctype vec3 hmm_vec3

//=== shadow pass
@vs vs_shadow
@glsl_options fixup_clipspace // important: map clipspace z from -1..+1 to 0..+1 on GL

uniform vs_shadow_params {
    mat4 mvp;
};

in vec4 pos;

void main() {
    gl_Position = mvp * pos;
}
@end

@fs fs_shadow
void main() { }
@end

@program shadow vs_shadow fs_shadow

//=== display pass
@vs vs_display

uniform vs_display_params {
    mat4 mvp;
    mat4 model;
    mat4 light_mvp;
    vec3 diff_color;
};

in vec4 pos;
in vec3 norm;
out vec3 color;
out vec4 light_proj_pos;
out vec4 world_pos;
out vec3 world_norm;

void main() {
    gl_Position = mvp * pos;
    light_proj_pos = light_mvp * pos;
    #if !SOKOL_GLSL
        light_proj_pos.y = -light_proj_pos.y;
    #endif
    world_pos = model * pos;
    world_norm = normalize((model * vec4(norm, 0.0)).xyz);
    color = diff_color;
}
@end

@fs fs_display

uniform fs_display_params {
    vec3 light_dir;
    vec3 eye_pos;
};

uniform texture2D shadow_map;
uniform sampler shadow_sampler;

in vec3 color;
in vec4 light_proj_pos;
in vec4 world_pos;
in vec3 world_norm;

out vec4 frag_color;

vec4 gamma(vec4 c) {
    float p = 1.0 / 2.2;
    return vec4(pow(c.xyz, vec3(p)), c.w);
}

void main() {
    float spec_power = 2.2;
    float ambient_intensity = 0.25;
    vec3 l = normalize(light_dir);
    vec3 n = normalize(world_norm);
    float n_dot_l = dot(n, l);
    if (n_dot_l > 0.0) {

        vec3 light_pos = light_proj_pos.xyz / light_proj_pos.w;
        vec3 sm_pos = vec3((light_pos.xy + 1.0) * 0.5, light_pos.z);
        float s = texture(sampler2DShadow(shadow_map, shadow_sampler), sm_pos);
        float diff_intensity = max(n_dot_l * s, 0.0);

        vec3 v = normalize(eye_pos - world_pos.xyz);
        vec3 r = reflect(-l, n);
        float r_dot_v = max(dot(r, v), 0.0);
        float spec_intensity = pow(r_dot_v, spec_power) * n_dot_l * s;

        frag_color = vec4(vec3(spec_intensity) + (diff_intensity + ambient_intensity) * color, 1.0);
    } else {
        frag_color = vec4(color * ambient_intensity, 1.0);
    }
    frag_color = gamma(frag_color);
}
@end

@program display vs_display fs_display

//=== debug visualization sampler to render shadow map as regular texture
@vs vs_dbg
@glsl_options flip_vert_y

in vec2 pos;
out vec2 uv;

void main() {
    gl_Position = vec4(pos*2.0 - 1.0, 0.5, 1.0);
    uv = pos;
}
@end

@fs fs_dbg
@image_sample_type dbg_tex unfilterable_float
@sampler_type dbg_smp nonfiltering
uniform texture2D dbg_tex;
uniform sampler dbg_smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(sampler2D(dbg_tex, dbg_smp), uv).xxx, 1.0);
}
@end

@program dbg vs_dbg fs_dbg
