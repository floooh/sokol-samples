//------------------------------------------------------------------------------
//  shaders for mrt-sapp sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2

// shaders for offscreen-pass rendering
@vs vs_offscreen

uniform offscreen_params {
    mat4 mvp;
};

in vec4 pos;
in float bright0;

out float bright;

void main() {
    gl_Position = mvp * pos;
    bright = bright0;
}
@end

@fs fs_offscreen
in float bright;

layout(location=0) out vec4 frag_color_0;
layout(location=1) out vec4 frag_color_1;
layout(location=2) out vec4 frag_color_2;

void main() {
    frag_color_0 = vec4(bright, 0.0, 0.0, 1.0);
    frag_color_1 = vec4(0.0, bright, 0.0, 1.0);
    frag_color_2 = vec4(0.0, 0.0, bright, 1.0);
}
@end

@program offscreen vs_offscreen fs_offscreen

// shaders for rendering a fullscreen-quad in default pass
@vs vs_fsq
@glsl_options flip_vert_y

uniform fsq_params {
    vec2 offset;
};

in vec2 pos;

out vec2 uv0;
out vec2 uv1;
out vec2 uv2;

void main() {
    gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);
    uv0 = pos + vec2(offset.x, 0.0);
    uv1 = pos + vec2(0.0, offset.y);
    uv2 = pos;
}
@end

@fs fs_fsq
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;

in vec2 uv0;
in vec2 uv1;
in vec2 uv2;

out vec4 frag_color;

void main() {
    vec3 c0 = texture(tex0, uv0).xyz;
    vec3 c1 = texture(tex1, uv1).xyz;
    vec3 c2 = texture(tex2, uv2).xyz;
    frag_color = vec4(c0 + c1 + c2, 1.0);
}
@end

@program fsq vs_fsq fs_fsq

// shaders for rendering a debug visualization
@vs vs_dbg
@glsl_options flip_vert_y

in vec2 pos;
out vec2 uv;

void main() {
    gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);
    uv = pos;
}
@end

@fs fs_dbg
uniform sampler2D tex;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(tex,uv).xyz, 1.0);
}
@end

@program dbg vs_dbg fs_dbg


