@ctype mat4 hmm_mat4

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
    float scale;
};

in vec4 position;
out vec3 uvw;

void main() {
    gl_Position = mvp * position;
    uvw = ((position.xyz * scale) + 1.0) * 0.5;
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

@program cube vs fs
