@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    float draw_mode;
    mat4 mvp;
};

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=3) in vec4 color0;

out vec4 color;

void main() {
    gl_Position = mvp * position;
    if (draw_mode == 0.0) {
        color = vec4((normal + 1.0) * 0.5, 1.0);
    }
    else if (draw_mode == 1.0) {
        color = vec4(texcoord, 0.0, 1.0);
    }
    else {
        color = color0;
    }
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
