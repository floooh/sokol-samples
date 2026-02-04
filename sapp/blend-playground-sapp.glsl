@ctype mat4 mat44_t

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
