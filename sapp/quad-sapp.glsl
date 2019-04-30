/* quad vertex shader */
@vs vs
in vec4 position;
in vec4 color0;
out vec4 color;

void main() {
    gl_Position = position;
    color = color0;
}
@end

/* quad fragment shader */
@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

/* quad shader program */
@program quad vs fs

