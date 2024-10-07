//------------------------------------------------------------------------------
//  float/rgba8 encoding/decoding so that we can use an RGBA8
//  shadow map instead of floating point render targets which might
//  not be supported everywhere
//
//  http://aras-p.info/blog/2009/07/30/encoding-floats-to-rgba-the-final/
//
@ctype mat4 hmm_mat4
@ctype vec3 hmm_vec3
@ctype vec2 hmm_vec2

@block util

vec4 encode_depth(float v) {
    vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
    enc = fract(enc);
    enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
    return enc;
}

float decodeDepth(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));
}

float sample_shadow(texture2D tex, sampler smp, vec2 uv, float compare) {
    float depth = decodeDepth(texture(sampler2D(tex, smp), vec2(uv.x, uv.y)));
    return step(compare, depth);
}

float sample_shadow_pcf(texture2D tex, sampler smp, vec3 uv_depth, vec2 sm_size) {
    float result = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y =- 2; y <= 2; y++) {
            vec2 offset = vec2(x, y) / sm_size;
            result += sample_shadow(tex, smp, uv_depth.xy + offset, uv_depth.z);
        }
    }
    return result / 25.0;
}

@end

//=== shadow pass
@vs vs_shadow

layout(binding=0) uniform vs_shadow_params {
    mat4 mvp;
};

in vec4 pos;
out vec2 proj_zw;

void main() {
    gl_Position = mvp * pos;
    proj_zw = gl_Position.zw;
}
@end

@fs fs_shadow
@include_block util

in vec2 proj_zw;
out vec4 frag_color;

void main() {
    float depth = proj_zw.x / proj_zw.y;
    frag_color = encode_depth(depth);
}
@end

@program shadow vs_shadow fs_shadow

//=== display pass
@vs vs_display

layout(binding=0) uniform vs_display_params {
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
@include_block util

layout(binding=1) uniform fs_display_params {
    vec3 light_dir;
    vec3 eye_pos;
};

layout(binding=0) uniform texture2D shadow_map;
layout(binding=0) uniform sampler shadow_sampler;

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
    vec2 sm_size = textureSize(sampler2D(shadow_map, shadow_sampler), 0);
    float spec_power = 2.2;
    float ambient_intensity = 0.25;
    vec3 l = normalize(light_dir);
    vec3 n = normalize(world_norm);
    float n_dot_l = dot(n, l);
    if (n_dot_l > 0.0) {

        vec3 light_pos = light_proj_pos.xyz / light_proj_pos.w;
        vec3 sm_pos = vec3((light_pos.xy + 1.0) * 0.5, light_pos.z);
        float s = sample_shadow_pcf(shadow_map, shadow_sampler, sm_pos, sm_size);
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
@include_block util

layout(binding=0) uniform texture2D dbg_tex;
layout(binding=0) uniform sampler dbg_smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    float depth = decodeDepth(texture(sampler2D(dbg_tex, dbg_smp), uv));
    frag_color = vec4(depth, depth, depth, 1.0);
}
@end

@program dbg vs_dbg fs_dbg
