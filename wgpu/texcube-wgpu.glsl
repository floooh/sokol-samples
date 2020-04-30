//------------------------------------------------------------------------------
//  Shader code for texcube-wgpu sample.
//
//  NOTE: This source file also uses the '#pragma sokol' form of the
//  custom tags.
//------------------------------------------------------------------------------
#pragma sokol @ctype mat4 hmm_mat4

#pragma sokol @vs vs
uniform vs_params {
    mat4 mvp;
};

in vec4 pos;
in vec4 color0;
in vec2 texcoord0;

out vec4 color;
out vec2 uv;

void main() {
    gl_Position = mvp * pos;
    color = color0;
    uv = texcoord0 * 5.0;
}
#pragma sokol @end

#pragma sokol @fs fs
layout(binding=0) uniform sampler2D tex;

in vec4 color;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(tex, uv) * color;
}
#pragma sokol @end

#pragma sokol @program texcube vs fs
