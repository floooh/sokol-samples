@ctype mat4 hmm_mat4

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
};

// NOTE: 'vertex' is a reserved name in MSL
struct sb_vertex {
    vec3 pos;
    vec4 color;
};

layout(binding=0) readonly buffer ssbo {
    sb_vertex vtx[];
};

out vec4 color;

void main() {
    vec4 position = vec4(vtx[gl_VertexIndex].pos, 1.0);
    gl_Position = mvp * position;
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

@program vertexpull vs fs
