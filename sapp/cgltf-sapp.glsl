@ctype mat4 hmm_mat4
@ctype vec4 hmm_vec4
@ctype vec3 hmm_vec3

@vs vs
uniform vs_params {
    mat4 mvp;
};

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;

void main() {
    gl_Position = mvp * position;
}
@end

@fs metallic_fs
out vec4 frag_color;

uniform metallic_params {
    vec4 base_color_factor;
    vec3 emissive_factor;
    float metallic_factor;
    float roughness_factor;
};

uniform sampler2D base_color_texture;
uniform sampler2D metallic_roughness_texture;
uniform sampler2D normal_texture;
uniform sampler2D occlusion_texture;
uniform sampler2D emissive_texture;

void main() {
    frag_color = base_color_factor;
}
@end

@fs specular_fs
out vec4 frag_color;

uniform specular_params {
    vec4 diffuse_factor;
    vec3 specular_factor;
    vec3 emissive_factor;
    float glossiness_factor;
};

uniform sampler2D diffuse_texture;
uniform sampler2D specular_glossiness_texture;
uniform sampler2D normal_texture;
uniform sampler2D occlusion_texture;
uniform sampler2D emissive_texture;

void main() {
    frag_color = diffuse_factor;
}
@end

@program cgltf_metallic vs metallic_fs
@program cgltf_specular vs specular_fs

