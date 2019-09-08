
//------------------------------------------------------------------------------
//  Shader code for uvwrap-sapp sample.
//------------------------------------------------------------------------------
@vs vs
uniform vs_params {
    vec2 offset;
    vec2 scale;
};

in vec4 pos;
out vec2 uv;

void main() {
    gl_Position = vec4(pos.xy * scale + offset, 0.5, 1.0);
    uv = (pos.xy + 1.0) - 0.5;
}
@end

@fs fs
uniform sampler2D tex;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(tex, uv);
}
@end

@program uvwrap vs fs

