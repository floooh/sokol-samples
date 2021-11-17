@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
    float point_size;
};

in vec2 position;
in vec4 color0;
out vec4 color;

void main() {
    gl_Position = mvp * vec4(position.xy, 0, 1);
    gl_PointSize = point_size;
    color = color0;
}
@end

@fs fs
in vec4 color;
out vec4 fragColor;
void main() {
    fragColor = color;
}
@end

@program primtypes vs fs

