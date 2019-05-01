//------------------------------------------------------------------------------
//  shaders for blend-sapp sample
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4

@vs vs_bg
in vec2 position;
void main() {
    gl_Position = vec4(position, 0.5, 1.0);
}
@end

@fs fs_bg
uniform bg_fs_params {
    float tick;
};

out vec4 frag_color;

void main() {
    vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);
    frag_color = vec4(vec3(xy.x*xy.y), 1.0);
}
@end

@program bg vs_bg fs_bg

@vs vs_quad
uniform quad_vs_params {
    mat4 mvp;
};

in vec4 position;
in vec4 color0;

out vec4 color;

void main() {
    gl_Position = mvp * position;
    color = color0;
}
@end

@fs fs_quad
in vec4 color;
out vec4 frag_color;
void main() {
    frag_color = color;
}
@end

@program quad vs_quad fs_quad
