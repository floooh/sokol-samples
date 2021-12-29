@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2
@ctype vec3 hmm_vec3
@ctype vec4 hmm_vec4

@block skin_utils
void skinned_pos_nrm(in vec4 pos, in vec4 nrm, in vec4 skin_weights, in vec4 skin_indices, in vec2 joint_uv, out vec4 skin_pos, out vec4 skin_nrm) {
    skin_pos = vec4(0.0, 0.0, 0.0, 1.0);
    skin_nrm = vec4(0.0, 0.0, 0.0, 0.0);    
    vec4 weights = skin_weights / dot(skin_weights, vec4(1.0));
    vec2 step = vec2(joint_pixel_width, 0.0);
    vec2 uv;
    vec4 xxxx, yyyy, zzzz;
    if (weights.x > 0.0) {
        uv = vec2(joint_uv.x + (3.0 * skin_indices.x)*joint_pixel_width, joint_uv.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.x;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.x;
    }
    if (weights.y > 0.0) {
        uv = vec2(joint_uv.x + (3.0 * skin_indices.y)*joint_pixel_width, joint_uv.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.y;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.y;
    }
    if (weights.z > 0.0) {
        uv = vec2(joint_uv.x + (3.0 * skin_indices.z)*joint_pixel_width, joint_uv.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.z;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.z;
    }
    if (weights.w > 0.0) {
        uv = vec2(joint_uv.x + (3.0 * skin_indices.w)*joint_pixel_width, joint_uv.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.w;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.w;
    }
}
@end

@block light_utils

#if defined(LIGHTING) || defined(MATERIAL)
uniform phong_params {
    #ifdef LIGHTING
    vec3 light_dir;
    vec3 eye_pos;
    vec3 light_color;
    #endif
    #ifdef MATERIAL
    vec3 mat_diffuse;
    vec3 mat_specular;
    float mat_spec_power;
    #endif
};
#endif

vec4 gamma(vec4 c) {
    float p = 1.0/2.2;
    return vec4(pow(c.xyz, vec3(p,p,p)), c.w);
}

vec4 phong(vec3 pos, vec3 nrm, vec3 l, vec3 eye, vec3 lcolor, vec3 diffuse, vec3 specular, float spec_power) {
    vec3 n = normalize(nrm);
    vec3 v = normalize(eye - pos);
    float n_dot_l = max(dot(n, l), 0.0);
    vec3 r = reflect(-l, n);
    float r_dot_v = max(dot(r, v), 0.0);
    float diff = n_dot_l;
    float spec = pow(r_dot_v, spec_power) * n_dot_l;
    vec4 color = vec4(lcolor * (specular * spec + diffuse * diff), 1.0);
    return color;
}
@end

@vs vs
uniform vs_params {
    mat4 mvp;
    mat4 model;
    #ifdef SKINNING
    vec2 joint_uv;
    float joint_pixel_width;
    #endif
};

in vec4 position;
in vec3 normal;
#ifdef SKINNING
uniform sampler2D joint_tex;
in vec4 jindices;
in vec4 jweights;
@include_block skin_utils
#endif

#ifdef LIGHTING
out vec3 P;
#endif
#if defined(LIGHTING) || !defined(MATERIAL)
out vec3 N;
#endif

void main() {
    // compute skinned model-space position and normal
    vec4 pos, nrm;
    #ifdef SKINNING
    skinned_pos_nrm(position, vec4(normal, 0.0), jweights, jindices * 255.0, joint_uv, pos, nrm);
    #else
    pos = position;
    nrm = vec4(normal, 0.0);
    #endif

    gl_Position = mvp * pos;
    #ifdef LIGHTING
    P = (model * pos).xyz;
    #endif
    #if defined(LIGHTING) || !defined(MATERIAL)
    N = (model * nrm).xyz;
    #endif
}
@end

@fs fs
@include_block light_utils

#ifdef LIGHTING
in vec3 P;
#endif
#if defined(LIGHTING) || !defined(MATERIAL)
in vec3 N;
#endif
out vec4 frag_color;

void main() {
    #ifdef MATERIAL
    vec3 diffuse = mat_diffuse;
    vec3 specular = mat_specular;
    float spec_power = mat_spec_power; 
    #else
    vec3 diffuse = N * 0.5 + 0.5;
    vec3 specular = vec3(1.0, 1.0, 1.0);
    float spec_power = 16.0;
    #endif
    #ifdef LIGHTING
    frag_color = gamma(phong(P, N, light_dir, eye_pos, light_color, diffuse, specular, spec_power));
    #else
    frag_color = vec4(diffuse, 1.0);
    #endif
}
@end

@program prog vs fs
