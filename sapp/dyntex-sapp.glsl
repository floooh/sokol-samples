//------------------------------------------------------------------------------
//  shaders for dyntex-sapp sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4

@vs vs
layout(binding=0) uniform vs_params {
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
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
layout(location=0) in vec4 color;
layout(location=1) in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv) * color;
}
@end

@program dyntex vs fs
