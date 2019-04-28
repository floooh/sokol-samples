@type mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

layout(location=0) in vec4 position;
layout(location=1) in vec4 color0;

layout(location=0) out vec4 color;

void main() {
    gl_Position = mvp * position;
    color = color0;
}
@end

@fs fs
layout(location=0) in vec4 color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program cube vs fs
