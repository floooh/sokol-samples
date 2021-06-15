@ctype mat4 hmm_mat4
@ctype vec4 hmm_vec4

@block skin_utils
void skinned_pos_nrm(in vec4 pos, in vec4 nrm, in vec4 skin_weights, in vec4 skin_indices, in vec4 skin_info, out vec4 skin_pos, out vec4 skin_nrm) {
    skin_pos = vec4(0.0, 0.0, 0.0, 1.0);
    skin_nrm = vec4(0.0, 0.0, 0.0, 0.0);    
    vec4 weights = skin_weights / dot(skin_weights, vec4(1.0));
    vec2 step = vec2(1.0 / skin_info.z, 0.0);
    vec2 uv;
    vec4 xxxx, yyyy, zzzz;
    if (weights.x > 0.0) {
        uv = vec2(skin_info.x + (3.0 * skin_indices.x)/skin_info.z, skin_info.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.x;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.x;
    }
    if (weights.y > 0.0) {
        uv = vec2(skin_info.x + (3.0 * skin_indices.y)/skin_info.z, skin_info.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.y;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.y;
    }
    if (weights.z > 0.0) {
        uv = vec2(skin_info.x + (3.0 * skin_indices.z)/skin_info.z, skin_info.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.z;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.z;
    }
    if (weights.w > 0.0) {
        uv = vec2(skin_info.x + (3.0 * skin_indices.w)/skin_info.z, skin_info.y);
        xxxx = textureLod(joint_tex, uv, 0.0);
        yyyy = textureLod(joint_tex, uv + step, 0.0);
        zzzz = textureLod(joint_tex, uv + 2.0 * step, 0.0);
        skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.w;
        skin_nrm.xyz += vec3(dot(nrm,xxxx), dot(nrm,yyyy), dot(nrm,zzzz)) * weights.w;
    }
}
@end

@vs vs
uniform vs_params {
    mat4 view_proj;
};

uniform sampler2D joint_tex;

in vec4 position;
in vec4 normal;
in vec4 jindices;
in vec4 jweights;
in vec4 inst_xxxx;
in vec4 inst_yyyy;
in vec4 inst_zzzz;
in vec4 inst_skin_info;

out vec3 color;

@include_block skin_utils

void main() {
    vec4 pos, nrm;
    skinned_pos_nrm(position, normal, jweights, jindices, inst_skin_info, pos, nrm);
    // FIXME: compute model-view-proj matrix
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
