// renders a bufferless 'hello-triangle'
@vs triangle_vs

const vec4 colors[3] = {
    vec4(1, 0, 0, 1),
    vec4(0, 1, 0, 1),
    vec4(0, 0, 1, 1),
};
const vec2 positions[3] = {
    vec2(0.0, 0.5),
    vec2(0.5, -0.5),
    vec2(-0.5, -0.5),
};

out vec4 color;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    color = colors[gl_VertexIndex];
}
@end

@fs triangle_fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program triangle triangle_vs triangle_fs

// renders a bufferless "fullscreen triangle"
@vs fullscreen_vs
@glsl_options flip_vert_y
out vec2 uv;

void main() {
    float x = (gl_VertexIndex & 1) != 0 ? 2.0 : 0.0;
    float y = (gl_VertexIndex & 2) != 0 ? 2.0 : 0.0;
    gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.0, 1.0);
    uv = vec2(x, 1.0 - y);
}
@end

@fs fullscreen_fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv);
}
@end

@program fullscreen fullscreen_vs fullscreen_fs
