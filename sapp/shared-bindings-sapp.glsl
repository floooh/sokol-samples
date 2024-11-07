@ctype mat4 hmm_mat4

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
};

layout(location=0) in vec4 pos;
layout(location=1) in vec4 color0;
layout(location=2) in vec2 texcoord0;

out vec4 color;
out vec2 uv;

void main() {
    gl_Position = mvp * pos;
    color = color0;
    uv = texcoord0 * 5.0;
}
@end

@fs fs_red
layout(binding=0) uniform texture2D tex_red;
layout(binding=8) uniform sampler smp_red;

in vec4 color;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex_red, smp_red), uv) * color;
}
@end

@fs fs_green
layout(binding=2) uniform texture2D tex_green;
layout(binding=4) uniform sampler smp_green;

in vec4 color;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex_green, smp_green), uv) * color;
}
@end

@fs fs_blue
layout(binding=4) uniform texture2D tex_blue;
layout(binding=2) uniform sampler smp_blue;

in vec4 color;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex_blue, smp_blue), uv) * color;
}
@end

@program red vs fs_red
@program green vs fs_green
@program blue vs fs_blue
