@vs vs
in vec2 pos;
in vec3 color0;
in vec2 inst_pos;

out vec4 color;

void main() {
    gl_Position = vec4(pos + inst_pos, 0, 1);
    color = vec4(color0, 1);
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program drawex vs fs
