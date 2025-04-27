// ported from WGSL: https://webgpu.github.io/webgpu-samples/?sample=imageBlur#blur.wgsl

@cs cs

layout(binding=0) uniform cs_params {
    int filter_dim;
    int block_dim;
    int flip;
};

layout(binding=0) uniform texture2D inp_tex;
layout(binding=0, rgba8) uniform writeonly image2D outp_tex;
layout(binding=0) uniform sampler smp;

layout(local_size_x=32, local_size_y=1, local_size_z=1) in;

shared vec3 tile[4][128];

void main() {
    int filter_offset = (filter_dim - 1) / 2;
    ivec2 dims = textureSize(sampler2D(inp_tex, smp), 0);
    ivec2 base_index = ivec2(vec2(gl_WorkGroupID.xy * vec2(block_dim, 4) + gl_LocalInvocationID.xy * vec2(4, 1)) - vec2(filter_offset, 0));
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            vec2 load_index = base_index + vec2(c, r);
            if (flip != 0) {
                load_index = load_index.yx;
            }
            const vec2 uv = (load_index + vec2(0.25, 0.25)) / vec2(dims);
            tile[r][4 * gl_LocalInvocationID.x + c] = textureLod(sampler2D(inp_tex,smp), uv, 0).rgb;
        }
    }
    barrier();
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            ivec2 write_index = base_index + ivec2(c, r);
            if (flip != 0) {
                write_index = write_index.yx;
            }
            int center = 4 * int(gl_LocalInvocationID.x) + c;
            if ((center >= filter_offset) && (center < 128 - filter_offset) && all(lessThan(write_index, dims))) {
                vec3 acc = vec3(0);
                for (int f = 0; f < filter_dim; f++) {
                    int i = int(center + f - filter_offset);
                    acc = acc + (1.0 / filter_dim) * tile[r][i];
                }
                imageStore(outp_tex, write_index, vec4(acc, 1));
            }
        }
    }
}
@end

@program compute cs
