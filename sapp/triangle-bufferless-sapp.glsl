@vs vs

const vec4 colors[3] = {
    vec4(1, 1, 0, 1),
    vec4(0, 1, 1, 1),
    vec4(1, 0, 1, 1),
};
const vec3 positions[3] = {
    vec3(0.0, 0.5, 0.5),
    vec3(0.5, -0.5, 0.5),
    vec3(-0.5, -0.5, 0.5),
};

out vec4 color;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex & 3], 1.0);
    color = colors[gl_VertexIndex & 3];
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program triangle vs fs
