@ctype mat4 mat44_t
@ctype vec2 vec2_t
@ctype vec3 vec3_t
@ctype vec4 vec4_t

@block util
vec4 gamma(vec4 c) {
    float p = 1.0/2.2;
    return vec4(pow(c.xyz, vec3(p)), c.w);
}

float sample_shadow_pcf(texture2D tex, sampler smp, vec3 sm_pos) {
    vec2 sm_size = vec2(textureSize(sampler2DShadow(tex, smp), 0));
    float result = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y =- 2; y <= 2; y++) {
            vec2 offset = vec2(x, y) / sm_size;
            result += texture(sampler2DShadow(tex, smp), vec3(sm_pos.xy + offset, sm_pos.z));
        }
    }
    return result / 25.0;
}
@end

@vs shadow_inst_vs
@glsl_options fixup_clipspace // important: map clipspace z from -1..+1 to 0..+1 on GL
layout(binding=0) uniform shadow_inst_vs_params {
    mat4 light_view_proj;
};
layout(location=0) in vec4 pos;
layout(location=1) in vec4 inst_xxxx;
layout(location=2) in vec4 inst_yyyy;
layout(location=3) in vec4 inst_zzzz;
void main() {
    vec4 world_pos = vec4(dot(pos, inst_xxxx), dot(pos, inst_yyyy), dot(pos, inst_zzzz), 1.0);
    gl_Position = light_view_proj * world_pos;
}
@end

@fs shadow_fs
void main() {}
@end

@program shadow_instanced shadow_inst_vs shadow_fs

@vs display_vs
layout(binding=0) uniform display_vs_params {
    mat4 mvp;
    mat4 model;
    mat4 light_mvp;
    vec4 diff_color;
};

layout(location=0) in vec4 pos;
layout(location=1) in vec3 normal;

out vec3 color;
out vec4 light_proj_pos;    // light space position (for shadow mapping)
out vec4 world_pos;
out vec3 world_nrm;

void main() {
    gl_Position = mvp * pos;
    light_proj_pos = light_mvp * pos;
    #if !SOKOL_GLSL
        light_proj_pos.y = -light_proj_pos.y;
    #endif
    world_pos = model * pos;
    world_nrm = (model * vec4(normal, 0)).xyz;
    color = diff_color.xyz;
}
@end

@vs display_inst_vs
layout(binding=0) uniform display_inst_vs_params {
    mat4 view_proj;
    mat4 light_view_proj;
    float awake_filter;
};

layout(location=0) in vec4 pos;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 inst_xxxx;
layout(location=3) in vec4 inst_yyyy;
layout(location=4) in vec4 inst_zzzz;
layout(location=5) in vec4 inst_color;
out vec3 color;
out vec4 light_proj_pos;    // light space position (for shadow mapping)
out vec4 world_pos;
out vec3 world_nrm;

void main() {
    vec4 wp = vec4(dot(pos, inst_xxxx), dot(pos, inst_yyyy), dot(pos, inst_zzzz), 1.0);
    vec4 nrm4 = vec4(normal, 0.0);
    vec3 wn = vec3(dot(nrm4, inst_xxxx), dot(nrm4, inst_yyyy), dot(nrm4, inst_zzzz));
    gl_Position = view_proj * wp;
    light_proj_pos = light_view_proj * wp;
    #if !SOKOL_GLSL
        light_proj_pos.y = -light_proj_pos.y;
    #endif
    world_pos = wp;
    world_nrm = wn;
    color = mix(inst_color.xyz, vec3(0.25f, 0.25f, 0.25f), awake_filter * inst_color.w);
}
@end

@fs display_fs
@include_block util
layout(binding=1) uniform display_fs_params {
    vec3 light_dir;
    vec3 eye_pos;
};

layout(binding=0) uniform texture2D shadow_map;
layout(binding=0) uniform sampler shadow_sampler;

in vec3 color;
in vec4 light_proj_pos;
in vec4 world_pos;
in vec3 world_nrm;

out vec4 frag_color;

void main() {
    float spec_power = 16.0;
    float ambient_intensity = 0.25;

    // diffuse lighting
    vec3 l = light_dir;
    vec3 n = normalize(world_nrm);
    float n_dot_l = dot(n, l);
    if (n_dot_l > 0) {

        vec3 light_pos = light_proj_pos.xyz / light_proj_pos.w;
        float depth_bias = max(0.0001 * (1.0 - n_dot_l), 0.00001);
        vec3 sm_pos = vec3((light_pos.xy + 1.0) * 0.5, light_pos.z + depth_bias);
        float s = sample_shadow_pcf(shadow_map, shadow_sampler, sm_pos);

        float diff_intensity = max(n_dot_l * s, 0);
        vec3 v = normalize(eye_pos - world_pos.xyz);
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

@program display display_vs display_fs
@program display_instanced display_inst_vs display_fs
