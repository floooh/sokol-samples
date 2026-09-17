@vs vs
in vec2 position;
void main() {
    gl_Position = vec4(position, 0, 1);
}
@end

@fs fs
out vec4 frag_color;

void main() {
    frag_color = vec4(1.0, 1.0, 0.0, 1.0);
}
@end

@program scroller vs fs
