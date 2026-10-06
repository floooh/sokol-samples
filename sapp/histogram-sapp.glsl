@ctype mat4 mat44_t

@block hist_common
const int NUM_BINS = 256;
// one histogram bin with a counter per color channel
struct hist_bin {
    uint r;
    uint g;
    uint b;
    uint pad;
};
@end

// a compute shader to clear the history at the start of a frame
@cs cs_clear
@include_block hist_common
layout(binding=0) buffer hist_clear {
    hist_bin clear_bins[];
};
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main() {
    clear_bins[gl_GlobalInvocationID.x] = hist_bin(0, 0, 0, 0);
}
@end
@program clear cs_clear

// offscreen pass: render a shape and count fragment colors
@vs vs_shape
layout(binding=0) uniform vs_params {
    mat4 mvp;
};
in vec3 in_pos;
in vec4 in_color;
out vec4 color;

void main() {
    gl_Position = mvp * vec4(in_pos, 1.0);
    color = in_color;
}
@end

@fs fs_shape
@include_block hist_common
layout(binding=0) buffer hist_out {
    hist_bin out_bins[];
};
in vec4 color;
out vec4 frag_color;

uint bin_index(float c) {
    return min(uint(clamp(c, 0, 1) * float(NUM_BINS)), NUM_BINS-1);
}

void main() {
    atomicAdd(out_bins[bin_index(color.r)].r, 1);
    atomicAdd(out_bins[bin_index(color.g)].g, 1);
    atomicAdd(out_bins[bin_index(color.b)].b, 1);
    frag_color = vec4(color.rgb, 1);
}
@end
@program shape vs_shape fs_shape

// display pass: render the offscreen image as fullscreen triangle
@vs vs_display_canvas
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };
out vec2 uv;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    uv = (pos * vec2(1, -1) + 1) * 0.5;
}
@end

@fs fs_display_canvas
layout(binding=0) uniform texture2D disp_tex;
layout(binding=0) uniform sampler disp_smp;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(sampler2D(disp_tex, disp_smp), uv).xyz, 1);
}
@end
@program display_canvas vs_display_canvas fs_display_canvas
