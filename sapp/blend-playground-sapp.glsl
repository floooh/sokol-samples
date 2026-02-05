@ctype mat4 mat44_t
@ctype vec2 vec2_t

// draw a checkboard fullscreen background
@vs vs_bg
void main() {
    if (0 == gl_VertexIndex) {
        gl_Position = vec4(-1, -1, 0, 1);
    } else if (1 == gl_VertexIndex) {
        gl_Position = vec4(3, -1, 0, 1);
    } else {
        gl_Position = vec4(-1, 3, 0, 1);
    }
}
@end

@fs fs_bg
layout(binding=0) uniform bg_params {
    float dark, light;
};

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
@program bg vs_bg fs_bg

in vec2 in_pos;

@vs vs_img
layout(binding=0) uniform img_params {
    vec2 offset;
    vec2 scale;
    float alpha_scale;
};

const vec2 pos[] = { vec2(-1, -1), vec2(+1, -1), vec2(-1, +1), vec2(+1, +1) };

out vec2 uv;
out float amul;

void main() {
    const vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4((p * scale) + offset, 0, 1);
    uv = p * vec2(0.5, -0.5) + 0.5;
    amul = alpha_scale;
}
@end

@block fs_img_common
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
out vec4 frag_color;
in vec2 uv;
in float amul;
@end

@fs fs_img_std
@include_block fs_img_common
void main() {
    frag_color = texture(sampler2D(tex,smp), uv) * vec4(1, 1, 1, amul);
}
@end
@program img_std vs_img fs_img_std
