@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec4 position;
in vec3 normal;

out vec4 color;

void main() {
    gl_Position = mvp * position;
    color = vec4((normal + 1.0) * 0.5, 1.0);
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program shapes vs fs
