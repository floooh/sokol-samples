//------------------------------------------------------------------------------
//  shaders for dyntex3d-sapp sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4

@vs vs
layout(binding=0) uniform vs_params {
    float w;
};

const vec2 vertices[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
const int indices[6] = { 0, 1, 2, 2, 3, 0 };

out vec3 uvw;

void main() {
    int idx = indices[gl_VertexIndex];
    gl_Position = vec4(vertices[idx] - 0.5, 0.5, 1.0);
    uvw = vec3(vertices[idx], w);
}
@end

@fs fs
layout(binding=0) uniform texture3D tex;
layout(binding=0) uniform sampler smp;
in vec3 uvw;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler3D(tex, smp), uvw);
}
@end

@program dyntex3d vs fs
