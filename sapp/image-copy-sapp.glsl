@ctype mat4 mat44_t

// shader for rendering the rotating cube
@vs vs
layout(binding=0) uniform cube_vs_params {
    mat4 mvp;
};
in vec4 position;
in vec4 color0;
out vec4 color;
void main() {
    gl_Position = mvp * position;
    color = color0;
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;
void main() {
    frag_color = vec4(color.rgb, 1);
}
@end

@program cube vs fs

// shader for rendering a 'fullscreen triangle' from synthesized vertices
@vs display_vs
out vec2 uv;
void main() {
    float x = (gl_VertexIndex << 1) & 2;
    float y = gl_VertexIndex & 2;
    gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.0, 1.0);
    uv = vec2(x, 1.0 - y);
}
@end

@fs display_fs
in vec2 uv;
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(tex, smp), uv);
}
@end

@program display display_vs display_fs
