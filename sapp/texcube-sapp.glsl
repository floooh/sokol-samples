@type mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

layout(location=0) in vec4 pos;
layout(location=1) in vec4 color0;
layout(location=2) in vec2 texcoord0;

layout(location=0) out vec4 color;
layout(location=1) out vec2 uv;

void main() {
    gl_Position = mvp * pos;
    color = color0;
    uv = texcoord0 * 5.0;
}
@end

@fs fs
layout(binding=0) uniform sampler2D tex;

layout(location=0) in vec4 color;
layout(location=1) in vec2 uv;

layout(location=0) out vec4 frag_color;

void main() {
    frag_color = texture(tex, uv) * color;
}
@end

@program texcube vs fs

