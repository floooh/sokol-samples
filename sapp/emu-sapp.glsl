@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};
uniform sampler2D vs_tex;

in vec2 pos;
out vec2 uv;

void main() {
    vec3 c = texture(vs_tex, pos.xy).rgb;
    float l = max(c.r, max(c.g, c.b));
    vec3 p = vec3(-pos.yx * 2.0 + 1.0, l * 0.1);
    gl_Position = mvp * vec4(p, 1.0);
    uv = pos;
}
@end

@fs fs
uniform sampler2D fs_tex;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(fs_tex, uv);
}
@end

@program emu vs fs
