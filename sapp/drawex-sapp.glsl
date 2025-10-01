@vs vs
in vec2 pos;
in vec3 color0;
in vec3 inst_data;

out vec4 color;

void main() {
    vec2 inst_pos = inst_data.xy;
    float inst_scale = inst_data.z;
    gl_Position = vec4(pos * inst_scale + inst_pos, 0, 1);
    color = vec4(color0, 1);
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program drawex vs fs
