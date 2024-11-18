// triangle shader which renders into an msaa render target
@vs triangle_vs

const vec4 colors[3] = {
    vec4(1, 1, 0, 1),
    vec4(0, 1, 1, 1),
    vec4(1, 0, 1, 1),
};
const vec3 positions[3] = {
    vec3(0.0, 0.6, 0.0),
    vec3(0.5, -0.6, 0.0),
    vec3(-0.5, -0.4, 0.0),
};

out vec4 color;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 1.0);
    color = colors[gl_VertexIndex];
}
@end

@fs triangle_fs
in vec4 color;
out vec4 frag_color;

void main() {
    float a = 0;
    if ((gl_SampleMaskIn[0] & 15) == 15) {
        a = 1;
    }
    frag_color = vec4(color.xyz, a);
}
@end

@program msaa triangle_vs triangle_fs

@block fullscreen_triangle
const vec3 positions[3] = {
    vec3(-1.0, -1.0, 0.0),
    vec3(3.0, -1.0, 0.0),
    vec3(-1.0, 3.0, 0.0),
};
@end

// custom resolve shader
@vs resolve_vs
@include_block fullscreen_triangle

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 1);
}
@end

@fs resolve_fs
layout(binding=0) uniform texture2DMS texms;
layout(binding=0) uniform sampler smp;
layout(binding=0) uniform fs_params {
    float weight0;
    float weight1;
    float weight2;
    float weight3;
    int coverage;
};

out vec4 frag_color;

void main() {
    ivec2 uv = ivec2(gl_FragCoord.xy);
    vec4 s0 = texelFetch(sampler2DMS(texms, smp), uv, 0);
    vec4 s1 = texelFetch(sampler2DMS(texms, smp), uv, 1);
    vec4 s2 = texelFetch(sampler2DMS(texms, smp), uv, 2);
    vec4 s3 = texelFetch(sampler2DMS(texms, smp), uv, 3);
    if (coverage != 0) {
        if ((s0.w + s1.w + s2.w + s3.w) < 4) {
            // complex pixel
            frag_color = vec4(1, 0, 0, 1);
        } else {
            // simple pixel
            frag_color = vec4(0, 0, 0, 0);
        }
    } else {
        frag_color = vec4(s0.xyz*weight0 + s1.xyz*weight1 + s2.xyz*weight2 + s3.xyz*weight3, 1);
    }
}
@end

@program resolve resolve_vs resolve_fs

// the final display pass shader which renders the custom-resolved texture to the display
@vs display_vs
@glsl_options flip_vert_y
@include_block fullscreen_triangle

out vec2 uv;

void main() {
    const vec4 pos = vec4(positions[gl_VertexIndex], 1);
    gl_Position = pos;
    uv = (pos.xy + 1.0) * 0.5;
}
@end

@fs display_fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv);
}
@end

@program display display_vs display_fs
