@ctype mat4 hmm_mat4
@ctype vec3 hmm_vec3
@ctype vec4 hmm_vec4

@vs vs
uniform vs_params {
    mat4 mvp;
};

struct sb_vertex {
    vec3 pos;
    vec4 color;
};

struct sb_instance {
    vec3 pos;
};

readonly buffer vertices {
    sb_vertex vtx[];
};

readonly buffer instances {
    sb_instance inst[];
};

out vec4 color;

void main() {
    const vec4 pos = vec4(vtx[gl_VertexIndex].pos + inst[gl_InstanceIndex].pos, 1.0);
    gl_Position = mvp * pos;
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

@program instancing vs fs
