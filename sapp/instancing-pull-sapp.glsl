@ctype mat4 mat44_t
@ctype vec3 vec3_t
@ctype vec4 vec4_t

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
};

struct sb_vertex {
    vec3 pos;
    vec4 color;
};

struct sb_instance {
    vec3 pos;
};

layout(binding=0) readonly buffer vertices {
    sb_vertex vtx[];
};

layout(binding=1) readonly buffer instances {
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
