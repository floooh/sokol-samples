@vs vs
layout(binding = 0) uniform vs_params {
    vec2 offset;
};

in vec2 position;

void main() {
    gl_Position = vec4(position + offset * 0.5, 0.0, 1.0);
}
@end

@fs dense_fs
layout(binding = 1) uniform dense_fs_params {
    vec4 color;
};

out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@fs sparse_fs
layout(binding = 2) uniform sparse_fs_params {
    vec4 color;
};

out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program dense vs dense_fs
@program sparse vs sparse_fs
