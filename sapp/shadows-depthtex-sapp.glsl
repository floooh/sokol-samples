@ctype mat4 hmm_mat4
@ctype vec3 hmm_vec3

// shadow pass
@vs vs_shadow

uniform vs_shadow_params {
    mat4 mvp;
};

in vec4 pos;
void main() {
    gl_Position = mvp * pos;
}
@end

@fs fs_shadow
void main() {
    discard;
}
@end

@program shadow vs_shadow fs_shadow

// display pass
@vs vs_display

uniform vs_display_params {
    mat4 mvp;
};

in vec4 pos;
in vec3 norm;

void main() {
    gl_Position = mvp * pos;
}
@end

@fs fs_display

uniform texture2D shadow_map;
uniform sampler shadow_sampler;

out vec4 frag_color;

void main() {
    float s = texture(sampler2DShadow(shadow_map, shadow_sampler), vec3(0, 0, 1));
    frag_color = vec4(s, s, s, 1.0);
}
@end

@program display vs_display fs_display
