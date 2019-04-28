@vs vs
layout(location=0) in vec4 position;
layout(location=1) in vec4 color_in;

layout(location=0) out vec4 color_out;

void main() {
    gl_Position = position;
    color_out = color_in;
}
@end

@fs fs
layout(location=0) in vec4 color_in;
layout(location=0) out vec4 color_out;

void main() {
    color_out = color_in;
}
@end

@program bufferoffsets vs fs

