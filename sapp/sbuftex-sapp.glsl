@ctype mat4 hmm_mat4

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
};

struct sb_vertex {
    vec3 pos;
    uint idx;
    vec2 uv;
};

layout(binding=0) readonly buffer vertices {
    sb_vertex vtx[];
};

out vec3 uv_idx;

void main() {
    gl_Position = mvp * vec4(vtx[gl_VertexIndex].pos, 1.0);
    // need to add a bit of wiggle room here to prevent a precision problem on NVIDIA
    // down in the pixel shader
    uv_idx = vec3(vtx[gl_VertexIndex].uv, float(vtx[gl_VertexIndex].idx) + 0.5);
}
@end

@fs fs
layout(binding=1) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

struct sb_color {
    vec4 color;
};

layout(binding=2) readonly buffer colors {
    sb_color clr[];
};

in vec3 uv_idx;
out vec4 frag_color;

void main() {
    uint idx = uint(uv_idx.z);
    vec2 uv = uv_idx.xy;
    frag_color = vec4(texture(sampler2D(tex,smp), uv).xxx, 1.0) * clr[idx].color;
}
@end

@program sbuftex vs fs
