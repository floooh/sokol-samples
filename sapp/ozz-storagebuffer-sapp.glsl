@ctype mat4 hmm_mat4
@ctype vec4 hmm_vec4

@vs vs
uniform vs_params {
    mat4 view_proj;
};

struct sb_vertex {
    vec3 pos;
    uint normal;
    uint joint_indices;
    uint joint_weights;
};

// FIXME: turn this into a matrix
struct sb_instance {
    vec4 xxxx;
    vec4 yyyy;
    vec4 zzzz;
};

// FIXME: turn this into a matrix
struct sb_joint {
    vec4 xxxx;
    vec4 yyyy;
    vec4 zzzz;
};

readonly buffer vertices {
    sb_vertex vtx[];
};

readonly buffer instances {
    sb_instance inst[];
};

readonly buffer joints {
    sb_joint joint[];
};

out vec3 color;

void main() {
    // skinned model-space position and normal
    vec4 pos = vec4(vtx[gl_VertexIndex].pos, 1.0);
    vec4 nrm = unpackUnorm4x8(vtx[gl_VertexIndex].normal);
    vec4 jweights = unpackUnorm4x8(vtx[gl_VertexIndex].joint_weights);
    uint jindices = vtx[gl_VertexIndex].joint_indices;

    vec4 ixxxx = inst[gl_InstanceIndex].xxxx;
    vec4 iyyyy = inst[gl_InstanceIndex].yyyy;
    vec4 izzzz = inst[gl_InstanceIndex].zzzz;

    // transform pos and normal to world space
    pos = vec4(dot(pos, ixxxx), dot(pos, iyyyy), dot(pos, izzzz), 1.0);
    nrm = vec4(dot(nrm, ixxxx), dot(nrm, iyyyy), dot(pos, izzzz), 0.0);

    gl_Position = view_proj * pos;
    color = (nrm.xyz + 1.0) * 0.5;
}
@end

@fs fs
in vec3 color;
out vec4 frag_color;

void main() {
    frag_color = vec4(color, 1.0);
}
@end

@program skinned vs fs
