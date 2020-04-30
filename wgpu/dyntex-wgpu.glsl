//------------------------------------------------------------------------------
//  shaders for dyntex-wgpu sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec4 position;
in vec4 color0;
in vec2 texcoord0;

layout(location=1) out vec2 uv;
layout(location=0) out vec4 color;

void main() {
    gl_Position = mvp * position;
    uv = texcoord0;
    color = color0;
}
@end

@fs fs
uniform sampler2D tex;
layout(location=0) in vec4 color;
layout(location=1) in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(tex, uv) * color;
}
@end

@program dyntex vs fs
