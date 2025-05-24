// a simple compute shader to populate a storage image
@cs cs
layout(binding=0) uniform cs_params {
    float offset;
};
layout(binding=0, rgba8) uniform writeonly image2D cs_out_tex;
layout(local_size_x=16, local_size_y=16) in;

void main() {
    ivec2 size = imageSize(cs_out_tex);
    ivec2 pos = ivec2(mod(vec2(gl_GlobalInvocationID.xy) + vec2(size) * offset, size));
    vec4 color = vec4(vec2(gl_GlobalInvocationID.xy) / float(size), 0, 1);
    imageStore(cs_out_tex, pos, color);
}
@end
@program compute cs

// a regular vertex/fragment shader pair to display the result
@vs vs
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };
out vec2 uv;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    uv = (pos * vec2(1, -1) + 1) * 0.5;
}
@end
@fs fs
layout(binding=0) uniform texture2D disp_tex;
layout(binding=0) uniform sampler disp_smp;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(sampler2D(disp_tex, disp_smp), uv).xyz, 1);
}
@end
@program display vs fs
