//------------------------------------------------------------------------------
//  shaders for arraytex-sapp sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2

@vs vs
uniform vs_params {
    mat4 mvp;
    vec2 offset0;
    vec2 offset1;
    vec2 offset2;
};

in vec4 position;
in vec2 texcoord0;

out vec3 uv0;
out vec3 uv1;
out vec3 uv2;

void main() {
    gl_Position = mvp * position;
    uv0 = vec3(texcoord0 + offset0, 0.0);
    uv1 = vec3(texcoord0 + offset1, 1.0);
    uv2 = vec3(texcoord0 + offset2, 2.0);
}
@end

@fs fs
uniform texture2DArray tex;
uniform sampler smp;

in vec3 uv0;
in vec3 uv1;
in vec3 uv2;

out vec4 frag_color;

void main() {
    vec4 c0 = texture(sampler2DArray(tex, smp), uv0);
    vec4 c1 = texture(sampler2DArray(tex, smp), uv1);
    vec4 c2 = texture(sampler2DArray(tex, smp), uv2);
    frag_color = vec4(c0.xyz + c1.xyz + c2.xyz, 1.0);
}
@end

@program arraytex vs fs
