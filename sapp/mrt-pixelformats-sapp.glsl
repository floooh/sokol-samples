//------------------------------------------------------------------------------
//  mrt-pixelformats-sapp.glsl
//  Shaders for mrt-pixelformats-sapp.c
//------------------------------------------------------------------------------
@ctype mat4 hmm_mat4

//--- offscreen MRT shader
@vs vs_offscreen

uniform offscreen_params {
    mat4 mvp;
};

in vec4 in_pos;
in vec3 in_normal;
in vec4 in_color;

out vec4 vs_proj;
out vec4 vs_normal;
out vec4 vs_color;

void main() {
    gl_Position = mvp * in_pos;
    vs_proj = gl_Position;
    vs_normal = mvp * vec4(in_normal, 0.0);
    vs_color = in_color;
}
@end

@fs fs_offscreen

in vec4 vs_proj;
in vec4 vs_normal;
in vec4 vs_color;

layout(location=0) out vec4 frag_depth;
layout(location=1) out vec4 frag_normal;
layout(location=2) out vec4 frag_color;

void main() {
    frag_depth = vec4(vs_proj.z);
    frag_normal = vs_normal;
    frag_color = vs_color;
}
@end

@program offscreen vs_offscreen fs_offscreen

//--- quad shader
@vs vs_quad
@glsl_options flip_vert_y

in vec2 pos;
out vec2 uv;

void main() {
    gl_Position = vec4(pos * 2.0 - 1.0, 0.5, 1.0);
    uv = pos;
}
@end

@fs fs_quad
uniform sampler2D tex;

in vec2 uv;
out vec4 frag_color;

uniform quad_params {
    float color_bias;
    float color_scale;
};

void main() {
    frag_color = vec4((texture(tex, uv).xyz + color_bias) * color_scale, 1.0);
}
@end

@program quad vs_quad fs_quad
