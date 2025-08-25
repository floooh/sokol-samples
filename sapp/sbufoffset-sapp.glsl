//
// NOTE: don't do this sort of silly bulk-data-initialization in real-world code!
//
@ctype mat4 mat44_t

@block common
struct sb_vertex {
    vec3 pos;
    vec4 color;
};
@end

// a compute shader to initialize the index data block in the storage buffer
@cs cs_init_indices
struct sb_index {
    uint i;
};

layout(binding=0) buffer cs_idx_ssbo { sb_index idx[]; };
layout(local_size_x = 32) in;

const uint src_indices[36] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};
void main() {
    const uint i = gl_GlobalInvocationID.x;
    if (i < 36) {
        idx[i].i = src_indices[i];
    }
}
@end
@program init_indices cs_init_indices

// a compute shader to initialize the vertex data block in the storage buffer
@cs cs_init_vertices
@include_block common
layout(binding=0) buffer cs_vtx_ssbo { sb_vertex vtx[]; };
layout(local_size_x = 32) in;

const sb_vertex src_vertices[24] = {
    { vec3(-1.0, -1.0, -1.0),  vec4(1.0, 0.0, 0.0, 1.0) },
    { vec3( 1.0, -1.0, -1.0),  vec4(1.0, 0.0, 0.0, 1.0) },
    { vec3( 1.0,  1.0, -1.0),  vec4(1.0, 0.0, 0.0, 1.0) },
    { vec3(-1.0,  1.0, -1.0),  vec4(1.0, 0.0, 0.0, 1.0) },
    { vec3(-1.0, -1.0,  1.0),  vec4(0.0, 1.0, 0.0, 1.0) },
    { vec3( 1.0, -1.0,  1.0),  vec4(0.0, 1.0, 0.0, 1.0) },
    { vec3( 1.0,  1.0,  1.0),  vec4(0.0, 1.0, 0.0, 1.0) },
    { vec3(-1.0,  1.0,  1.0),  vec4(0.0, 1.0, 0.0, 1.0) },
    { vec3(-1.0, -1.0, -1.0),  vec4(0.0, 0.0, 1.0, 1.0) },
    { vec3(-1.0,  1.0, -1.0),  vec4(0.0, 0.0, 1.0, 1.0) },
    { vec3(-1.0,  1.0,  1.0),  vec4(0.0, 0.0, 1.0, 1.0) },
    { vec3(-1.0, -1.0,  1.0),  vec4(0.0, 0.0, 1.0, 1.0) },
    { vec3( 1.0, -1.0, -1.0),  vec4(1.0, 0.5, 0.0, 1.0) },
    { vec3( 1.0,  1.0, -1.0),  vec4(1.0, 0.5, 0.0, 1.0) },
    { vec3( 1.0,  1.0,  1.0),  vec4(1.0, 0.5, 0.0, 1.0) },
    { vec3( 1.0, -1.0,  1.0),  vec4(1.0, 0.5, 0.0, 1.0) },
    { vec3(-1.0, -1.0, -1.0),  vec4(0.0, 0.5, 1.0, 1.0) },
    { vec3(-1.0, -1.0,  1.0),  vec4(0.0, 0.5, 1.0, 1.0) },
    { vec3( 1.0, -1.0,  1.0),  vec4(0.0, 0.5, 1.0, 1.0) },
    { vec3( 1.0, -1.0, -1.0),  vec4(0.0, 0.5, 1.0, 1.0) },
    { vec3(-1.0,  1.0, -1.0),  vec4(1.0, 0.0, 0.5, 1.0) },
    { vec3(-1.0,  1.0,  1.0),  vec4(1.0, 0.0, 0.5, 1.0) },
    { vec3( 1.0,  1.0,  1.0),  vec4(1.0, 0.0, 0.5, 1.0) },
    { vec3( 1.0,  1.0, -1.0),  vec4(1.0, 0.0, 0.5, 1.0) },
};

void main() {
    const uint i = gl_GlobalInvocationID.x;
    if (i < 24) {
        vtx[i] = src_vertices[i];
    }
}
@end
@program init_vertices cs_init_vertices

// a vertex shader with vertex pulling
@vs vs
@include_block common

layout(binding=0) uniform vs_params { mat4 mvp; };
layout(binding=0) readonly buffer vs_vtx_ssbo { sb_vertex vtx[]; };

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
@program sbufoffset vs fs
