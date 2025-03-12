@ctype mat4 hmm_mat4
@ctype vec4 hmm_vec4

@block skin_utils
void skinned_pos_nrm(in vec4 pos, in vec4 nrm, in vec4 skin_weights, in uvec4 skin_indices, out vec4 skin_pos, out vec4 skin_nrm) {
    skin_pos = vec4(0.0, 0.0, 0.0, 1.0);
    skin_nrm = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 weights = skin_weights / dot(skin_weights, vec4(1.0));
    ivec2 uv;
    vec4 xxxx, yyyy, zzzz;
    if (weights.x > 0.0) {
        uv = ivec2(3 * skin_indices.x, gl_InstanceIndex);
        xxxx = texelFetch(sampler2D(joint_tex, smp), uv, 0);
        yyyy = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(1,0), 0);
        zzzz = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(2,0), 0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.x;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.x;
    }
    if (weights.y > 0.0) {
        uv = ivec2(3 * skin_indices.y, gl_InstanceIndex);
        xxxx = texelFetch(sampler2D(joint_tex, smp), uv, 0);
        yyyy = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(1,0), 0);
        zzzz = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(2,0), 0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.y;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.y;
    }
    if (weights.z > 0.0) {
        uv = ivec2(3 * skin_indices.z, gl_InstanceIndex);
        xxxx = texelFetch(sampler2D(joint_tex, smp), uv, 0);
        yyyy = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(1,0), 0);
        zzzz = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(2,0), 0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.z;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.z;
    }
    if (weights.w > 0.0) {
        uv = ivec2(3 * skin_indices.w, gl_InstanceIndex);
        xxxx = texelFetch(sampler2D(joint_tex, smp), uv, 0);
        yyyy = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(1,0), 0);
        zzzz = texelFetch(sampler2D(joint_tex, smp), uv + ivec2(2,0), 0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.w;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.w;
    }
}
@end

@vs vs
layout(binding=0) uniform vs_params {
    mat4 view_proj;
};

@image_sample_type joint_tex unfilterable_float
layout(binding=0) uniform texture2D joint_tex;
@sampler_type smp nonfiltering
layout(binding=0) uniform sampler smp;

in vec4 position;
in vec4 normal;
in uvec4 jindices;
in vec4 jweights;
in vec4 inst_xxxx;
in vec4 inst_yyyy;
in vec4 inst_zzzz;

out vec3 color;

@include_block skin_utils

void main() {
    // compute skinned model-space position and normal
    vec4 pos, nrm;
    skinned_pos_nrm(position, normal, jweights, jindices, pos, nrm);

    // transform pos and normal to world space
    pos = vec4(dot(pos,inst_xxxx), dot(pos,inst_yyyy), dot(pos,inst_zzzz), 1.0);
    nrm = vec4(dot(nrm,inst_xxxx), dot(nrm,inst_yyyy), dot(nrm,inst_zzzz), 0.0);

    gl_Position = view_proj * pos;
    color = (nrm.xyz + 1.0) * 0.5;
}
@end

@fs fs
in vec3 color;
out vec4 frag_color;

void main() {
    frag_color = vec4(color, 1.0);
}
@end

@program skinned vs fs
