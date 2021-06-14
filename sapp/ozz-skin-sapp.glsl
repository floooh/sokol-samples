@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec4 position;
in vec3 normal;
in vec4 jindices;
in vec4 jweights;

out vec3 color;

void main() {
    gl_Position = mvp * position;
    color = (normal.xyz + 1.0) * 0.5;
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
