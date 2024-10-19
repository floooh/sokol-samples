@ctype mat4 hmm_mat4

@block vs_uniforms
layout(binding=0) uniform vs_params {
    mat4 mvp;
};
@end

@vs vs_offscreen
@include_block vs_uniforms

in vec4 in_pos;
in vec3 in_nrm;
out vec3 nrm;

void main() {
    gl_Position = mvp * in_pos;
    nrm = in_nrm;
}
@end

@fs fs_offscreen
in vec3 nrm;
out vec4 frag_color;

void main() {
    frag_color = vec4(nrm * 0.5 + 0.5, 1.0);
}
@end

@program offscreen vs_offscreen fs_offscreen

@vs vs_display
@include_block vs_uniforms

in vec4 in_pos;
in vec2 in_uv;
out vec2 uv;

void main() {
    gl_Position = mvp * in_pos;
    uv = in_uv;
}
@end

@fs fs_display
layout(binding=0) uniform texture2DArray tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    vec3 c0 = texture(sampler2DArray(tex, smp), vec3(uv, 0)).xyz;
    vec3 c1 = texture(sampler2DArray(tex, smp), vec3(uv, 1)).xyz;
    vec3 c2 = texture(sampler2DArray(tex, smp), vec3(uv, 2)).xyz;
    frag_color = vec4((c0 + c1 + c2) * 0.34, 1.0);
}
@end

@program display vs_display fs_display
