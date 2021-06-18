//------------------------------------------------------------------------------
//  Shader code for loadpng-sapp sample.
//------------------------------------------------------------------------------
@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec3 pos;
in vec2 texcoord0;
in vec4 color0;

out vec2 uv;
out vec4 color;

void main() {
    gl_Position = mvp * vec4(pos, 1.0f);
    uv = texcoord0;
    color = color0;
}
@end

@fs fs
uniform sampler2D tex;

in vec2 uv;
in vec4 color;

out vec4 frag_color;

void main() {
    frag_color = color * texture(tex, uv);
}
@end

@program spritebatch vs fs


