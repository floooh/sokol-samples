/* quad vertex shader */
@vs vs
layout(location=0) in vec4 position;
layout(location=1) in vec4 color0;

layout(location=0) out vec4 color;

void main() {
    gl_Position = position;
    color = color0;
}
@end

/* quad fragment shader */
@fs fs
layout(location=0) in vec4 color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

/* quad shader program */
@program quad vs fs

