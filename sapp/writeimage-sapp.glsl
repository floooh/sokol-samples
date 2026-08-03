
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

// FIXME
@fs fs
in vec2 uv;
out vec4 frag_color;
void main() {
    frag_color = vec4(uv, 0.5f, 1.0f);
}
@end
