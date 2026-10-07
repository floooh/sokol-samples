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

// a compute shader to clear the histogram at the start of a frame
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
    mat4 mv;
};
in vec3 in_pos;
in vec2 in_uv;
out vec2 uv;
out vec3 view_pos;

void main() {
    gl_Position = mvp * vec4(in_pos, 1.0);
    view_pos = (mv * vec4(in_pos, 1.0)).xyz;
    uv = in_uv;
}
@end

@fs fs_shape
@include_block hist_common
layout(binding=0) buffer hist_out {
    hist_bin out_bins[];
};
layout(binding=1) uniform texture2D shape_tex;
layout(binding=0) uniform sampler shape_smp;

in vec2 uv;
in vec3 view_pos;
out vec4 frag_color;

uint bin_index(float c) {
    return min(uint(clamp(c, 0, 1) * float(NUM_BINS)), NUM_BINS-1);
}

// quick'n'dirty hardwired lighting so the histogram actually changes in interesting ways
float lighting() {
    // flat face normal from screen-space derivatives, flipped to face the viewer
    vec3 n = normalize(cross(dFdx(view_pos), dFdy(view_pos)));
    n = faceforward(n, view_pos, n);
    vec3 light_dir = normalize(vec3(-0.5, 1.0, 1.0));
    return 0.25 + 0.75 * max(dot(n, light_dir), 0.0);
}

void main() {
    float intensity = lighting();
    vec4 color = vec4(texture(sampler2D(shape_tex, shape_smp), uv).rgb * intensity, 1.0);
    frag_color = vec4(color.rgb, 1);

    // update histogram values in storagebuffer
    uint r_index = bin_index(color.r);
    uint g_index = bin_index(color.g);
    uint b_index = bin_index(color.b);
    if (r_index > 0) {
        uint r_cur = atomicAdd(out_bins[r_index].r, 1) + 1;
        atomicMax(out_bins[0].r, r_cur);
    }
    if (g_index > 0) {
        uint g_cur = atomicAdd(out_bins[g_index].g, 1) + 1;
        atomicMax(out_bins[0].g, g_cur);
    }
    if (b_index > 0) {
        uint b_cur = atomicAdd(out_bins[b_index].b, 1) + 1;
        atomicMax(out_bins[0].b, b_cur);
    }
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
layout(binding=0) uniform texture2D canvas_tex;
layout(binding=0) uniform sampler canvas_smp;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(sampler2D(canvas_tex, canvas_smp), uv).xyz, 1);
}
@end
@program display_canvas vs_display_canvas fs_display_canvas

// the histogram renderer as a bar of synthesized quads, the size is
// defined by the viewport
@vs vs_display_hist
@include_block hist_common
layout(binding=0) uniform vs_hist_params {
    int channel;    // 0: red, 1: green, 2: blue
};
layout(binding=0) readonly buffer hist_in {
    hist_bin in_bins[];
};
out vec4 color;

uint channel_value(hist_bin bin) {
    return (channel == 0) ? bin.r : ((channel == 1) ? bin.g : bin.b);
}

void main() {
    // instance 0 => bin 1 (bin 0 holds the max value)
    uint max_count = max(channel_value(in_bins[0]), 1);
    uint count = channel_value(in_bins[gl_InstanceIndex + 1]);
    float height = float(count) / float(max_count);
    // triangle strip: (0,0), (1,0), (0,1), (1,1)
    vec2 pos = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1);
    pos.x = (float(gl_InstanceIndex) + pos.x) / float(NUM_BINS - 1);
    pos.y *= height;
    gl_Position = vec4(pos * 2 - 1, 0, 1);
    color = vec4(0, 0, 0, 1);
    color[channel] = 1;
}
@end

@fs fs_display_hist
in vec4 color;
out vec4 frag_color;
void main() {
    frag_color = color;
}
@end
@program display_hist vs_display_hist fs_display_hist
