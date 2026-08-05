
// vertex shader for a bufferless 'fullscreen triangle'
@vs vs
out vec2 uv;
void main() {
    float x = (gl_VertexIndex & 1) != 0 ? 2.0 : 0.0;
    float y = (gl_VertexIndex & 2) != 0 ? 2.0 : 0.0;
    gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.5, 1.0);
    uv = vec2(x, 1.0 - y);
}
@end

@block fs_common
layout(binding=0) uniform fs_params {
    float miplevel;
    float slice;
};
@end

@fs fs_tex2d
@include_block fs_common

in vec2 uv;

layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

out vec4 frag_color;
void main() {
    frag_color = textureLod(sampler2D(tex, smp), uv, miplevel);
}
@end

@program tex2d vs fs_tex2d
