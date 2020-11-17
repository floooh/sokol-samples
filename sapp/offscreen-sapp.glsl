//------------------------------------------------------------------------------
//  shaders for offscreen-sapp sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4

// shared code for all shaders
@block uniforms
uniform vs_params {
    mat4 mvp;
};
@end

// offscreen rendering shaders
@vs vs_offscreen
@include_block uniforms

in vec4 position;
in vec4 normal;
out vec4 nrm;

void main() {
    gl_Position = mvp * position;
    nrm = normal;
}
@end

@fs fs_offscreen
in vec4 nrm;
out vec4 frag_color;

void main() {
    frag_color = vec4(nrm.xyz * 0.5 + 0.5, 1.0);
}
@end

@program offscreen vs_offscreen fs_offscreen

// default-pass shaders
@vs vs_default
@include_block uniforms

in vec4 position;
in vec4 normal;
in vec2 texcoord0;
out vec4 nrm;
out vec2 uv;

void main() {
    gl_Position = mvp * position;
    uv = texcoord0;
    nrm = mvp * normal;
}
@end

@fs fs_default
uniform sampler2D tex;

in vec4 nrm;
in vec2 uv;

out vec4 frag_color;

void main() {
    vec4 c = texture(tex, uv * vec2(20.0, 10.0));
    float l = clamp(dot(nrm.xyz, normalize(vec3(1.0, 1.0, -1.0))), 0.0, 1.0) * 2.0;
    frag_color = vec4(c.xyz * (l + 0.25), 1.0);
}
@end

@program default vs_default fs_default

