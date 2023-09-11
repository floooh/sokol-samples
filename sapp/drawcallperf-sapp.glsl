@ctype mat4 hmm_mat4
@ctype vec4 hmm_vec4

@vs vs
uniform vs_per_frame {
    mat4 viewproj;
};

uniform vs_per_instance {
    vec4 world_pos;
};

in vec3 in_pos;
in vec2 in_uv;
in float in_bright;
out vec2 uv;
out float bright;

void main() {
    gl_Position = viewproj * (world_pos + vec4(in_pos * 0.05, 1.0));
    uv = in_uv;
    bright = in_bright;
}
@end

@fs fs
uniform texture2D tex;
uniform sampler smp;

in vec2 uv;
in float bright;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(sampler2D(tex, smp), uv).xyz * bright, 1.0);
}
@end

@program drawcallperf vs fs
