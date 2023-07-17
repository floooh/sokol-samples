@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
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
uniform texture3D tex;
uniform sampler smp;

in vec3 uvw;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler3D(tex, smp), uvw);
}
@end

@program cube vs fs
