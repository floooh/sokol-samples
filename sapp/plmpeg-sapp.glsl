@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec4 pos;
in vec3 normal;
in vec2 texcoord;

out vec2 uv;

void main() {
    gl_Position = mvp * (pos * vec4(1.76, 1.0, 1.76, 1.0) + vec4(normal * 0.5, 0.0));
    uv = texcoord;
}
@end

@fs fs
uniform texture2D tex_y;
uniform texture2D tex_cb;
uniform texture2D tex_cr;
uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

mat4 rec601 = mat4(
    1.16438,  0.00000,  1.59603, -0.87079,
    1.16438, -0.39176, -0.81297,  0.52959,
    1.16438,  2.01723,  0.00000, -1.08139,
    0, 0, 0, 1
);

void main() {
    float y = texture(sampler2D(tex_y, smp), uv).r;
    float cb = texture(sampler2D(tex_cb, smp), uv).r;
    float cr = texture(sampler2D(tex_cr, smp), uv).r;
    frag_color = vec4(y, cb, cr, 1.0) * rec601;
}
@end

@program plmpeg vs fs

