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

in vec4 pos;
in vec4 color0;

out vec4 color;

void main() {
    gl_Position = mvp * pos;
    color = color0;
}
@end

@fs fs_offscreen
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program offscreen vs_offscreen fs_offscreen

// default-pass shaders
@vs vs_default
@include_block uniforms

in vec4 pos;
in vec4 color0;
in vec2 uv0;

out vec4 color;
out vec2 uv;

void main() {
    gl_Position = mvp * pos;
    color = color0;
    uv = uv0;
}
@end

@fs fs_default
uniform sampler2D tex;

in vec4 color;
in vec2 uv;

out vec4 frag_color;

void main() {
    frag_color = texture(tex, uv) + color * 0.5;
}
@end

@program default vs_default fs_default

