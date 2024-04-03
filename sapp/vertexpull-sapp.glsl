@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

struct vertex_t {
    vec3 pos;
    vec4 color;
};

readonly buffer ssbo {
    vertex_t vtx[];
};

out vec4 color;

void main() {
    gl_Position = mvp * vec4(vtx[gl_VertexIndex].pos, 1.0);
    color = vtx[gl_VertexIndex].color;
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end