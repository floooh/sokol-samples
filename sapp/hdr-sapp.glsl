@vs vs

const vec4 colors[3] = {
    vec4(1, 0, 0, 1),
    vec4(0, 1, 0, 1),
    vec4(0, 0, 1, 1),
};
const vec2 positions[3] = {
    vec2(0.0, 0.5),
    vec2(0.4, -0.5),
    vec2(-0.4, -0.5),
};

out vec4 color;

void main() {
    vec2 pos;
    vec4 c;
    if (gl_VertexIndex < 3) {
        // left triangle (non hdr)
        pos = positions[gl_VertexIndex];
        c = colors[gl_VertexIndex];
        pos.x -= 0.5;
    } else {
        // right triangle (hdr)
        pos = positions[gl_VertexIndex - 3];
        c = colors[gl_VertexIndex - 3];
        c.rgb *= 4;
        pos.x += 0.5;
    }
    gl_Position = vec4(pos, 0.0, 1.0);
    color = c;
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
