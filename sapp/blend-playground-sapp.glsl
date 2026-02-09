@ctype mat4 mat44_t
@ctype vec2 vec2_t
@ctype vec4 vec4_t

@vs vs_fsq
@glsl_options flip_vert_y
out vec2 uv;

// generic fullscreen shader
void main() {
    if (0 == gl_VertexIndex) {
        gl_Position = vec4(-1, +1, 0, 1);
        uv = vec2(0, 0);
    } else if (1 == gl_VertexIndex) {
        gl_Position = vec4(3, +1, 0, 1);
        uv = vec2(2, 0);
    } else {
        gl_Position = vec4(-1, -3, 0, 1);
        uv = vec2(0, 2);
    }
}
@end

// draw a checkboard fullscreen background
@fs fs_bg
layout(binding=0) uniform bg_params {
    float dark, light;
};
in vec2 uv; // unused
out vec4 frag_color;
void main() {
    uvec2 xy = uvec2(floor(gl_FragCoord.xy / 15));
    if (((xy.x ^ xy.y) & 1) == 0) {
        frag_color = vec4(vec3(light), 1);
    } else {
        frag_color = vec4(vec3(dark), 1);
    }
}
@end
@program bg vs_fsq fs_bg

// fullscreen-compose offscreen render target with background
@fs fs_compose
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex,smp), uv);
}
@end
@program compose vs_fsq fs_compose

in vec2 in_pos;

@vs vs_img
layout(binding=0) uniform img_vs_params {
    vec2 offset;
    vec2 scale;
};

const vec2 pos[] = { vec2(-1, -1), vec2(+1, -1), vec2(-1, +1), vec2(+1, +1) };

out vec2 uv;

void main() {
    const vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4((p * scale) + offset, 0, 1);
    uv = p * vec2(0.5, -0.5) + 0.5;
}
@end

@block fs_img_common
layout(binding=1) uniform img_fs_params {
    vec4 src1_color;
    float alpha_scale;
    int premultiplied_alpha;
};
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
@end

@fs fs_img_std
@include_block fs_img_common

out vec4 frag_color;

void main() {
    vec4 c = texture(sampler2D(tex,smp), uv);
    c.a *= alpha_scale;
    if (0 != premultiplied_alpha) {
        c.rgb *= c.a;
    }
    frag_color = c;
}
@end

@fs fs_img_dualsrc
@include_block fs_img_common

layout(location = 0, index = 0) out vec4 frag_color;
layout(location = 0, index = 1) out vec4 frag_blend;

void main() {
    vec4 c = texture(sampler2D(tex,smp), uv);
    c.a *= alpha_scale;
    if (0 != premultiplied_alpha) {
        c.rgb *= c.a;
    }
    frag_color = c;
    frag_blend = src1_color;
}
@end

@program img_std vs_img fs_img_std
@program img_dualsrc vs_img fs_img_dualsrc
