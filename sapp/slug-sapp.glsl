@ctype mat4 mat44_t
@ctype vec4 vec4_t

@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
    vec4 draw_rect;   // xy = screen position (pixels), zw = screen size (pixels)
    vec4 glyph_bbox;  // xy = min corner (glyph space), zw = max corner (glyph space)
};

in vec2 quad_pos;     // unit quad vertex [0,1]
out vec2 glyph_pos;   // fragment position in glyph space

void main(){
    vec2 screen_pos = draw_rect.xy + quad_pos * draw_rect.zw;
    gl_Position = mvp * vec4(screen_pos, 0.0, 1.0);
    glyph_pos = mix(glyph_bbox.xy, glyph_bbox.zw, quad_pos);
}
@end

@fs fs
@image_sample_type curve_tex unfilterable_float
layout(binding=0) uniform texture2D curve_tex;
@image_sample_type band_tex uint
layout(binding=1) uniform utexture2D band_tex;
@sampler_type point_sampler nonfiltering
layout(binding=0) uniform sampler point_sampler;

layout(binding=1) uniform fs_params {
    vec4 text_color;       // RGBA
    vec4 band_transform;   // xy = band_scale, zw = band_offset
    ivec4 glyph_params;    // x = glyph_loc_x, y = glyph_loc_y, z = max_band_x, w = max_band_y
};
in vec2 glyph_pos;
out vec4 frag_color;

ivec2 bandLoc(ivec2 base, int offset) {
    ivec2 pos = ivec2(base.x + offset, base.y);
    pos.y += pos.x >> 12;
    pos.x &= 4095;
    return pos;
}
uint calcRootCode(float y1, float y2, float y3) {
    uint s1 = floatBitsToUint(y1) >> 31u;
    uint s2 = floatBitsToUint(y2) >> 30u;
    uint s3 = floatBitsToUint(y3) >> 29u;
    uint combined = (s2 & 2u) | (s1 & ~2u);
    combined = (s3 & 4u) | (combined & ~4u);
    return (0x2E74u >> combined) & 0x0101u;
}
vec2 solveHoriz(vec4 points_01, vec2 point_2) {
    vec2 a = points_01.xy - points_01.zw * 2.0 + point_2;
    vec2 b = points_01.xy - points_01.zw;
    float inv_a = 1.0 / a.y;
    float half_inv_b = 0.5 / b.y;
    float discriminant = sqrt(max(b.y * b.y - a.y * points_01.y, 0.0));
    float t1 = (b.y - discriminant) * inv_a;
    float t2 = (b.y + discriminant) * inv_a;
    if (abs(a.y) < 1.0 / 65536.0) { t1 = points_01.y * half_inv_b; t2 = t1; }
    return vec2(
        (a.x * t1 - b.x * 2.0) * t1 + points_01.x,
        (a.x * t2 - b.x * 2.0) * t2 + points_01.x
    );
}
vec2 solveVert(vec4 points_01, vec2 point_2) {
    vec2 a = points_01.xy - points_01.zw * 2.0 + point_2;
    vec2 b = points_01.xy - points_01.zw;
    float inv_a = 1.0 / a.x;
    float half_inv_b = 0.5 / b.x;
    float discriminant = sqrt(max(b.x * b.x - a.x * points_01.x, 0.0));
    float t1 = (b.x - discriminant) * inv_a;
    float t2 = (b.x + discriminant) * inv_a;
    if (abs(a.x) < 1.0 / 65536.0) { t1 = points_01.x * half_inv_b; t2 = t1; }
    return vec2(
        (a.y * t1 - b.y * 2.0) * t1 + points_01.y,
        (a.y * t2 - b.y * 2.0) * t2 + points_01.y
    );
}
void main() {
    ivec2 glyph_loc = glyph_params.xy;
    int max_band_x = glyph_params.z;
    int max_band_y = glyph_params.w;
    vec2 glyph_units_per_pixel = 1.0 / fwidth(glyph_pos);
    ivec2 band_index = clamp(
        ivec2(glyph_pos * band_transform.xy + band_transform.zw),
        ivec2(0, 0),
        ivec2(max_band_x, max_band_y)
    );
    // Horizontal bands (ray in +X)
    float h_winding = 0.0;
    float h_edge_weight = 0.0;
    {
        uvec4 band_header = texelFetch(usampler2D(band_tex, point_sampler),
                                       ivec2(glyph_loc.x + band_index.y, glyph_loc.y), 0);
        int curve_count = int(band_header.x);
        ivec2 entry_list_start = bandLoc(glyph_loc, int(band_header.y));
        for (int i = 0; i < curve_count && i < 128; i++) {
            ivec2 entry_coord = bandLoc(entry_list_start, i);
            uvec4 band_entry = texelFetch(usampler2D(band_tex, point_sampler), entry_coord, 0);
            ivec2 curve_tex_coord = ivec2(band_entry.x, band_entry.y);
            vec4 points_01 = texelFetch(sampler2D(curve_tex, point_sampler), curve_tex_coord, 0)
                            - vec4(glyph_pos, glyph_pos);
            ivec2 next_tex_coord = bandLoc(curve_tex_coord, 1);
            vec2 point_2 = texelFetch(sampler2D(curve_tex, point_sampler), next_tex_coord, 0).xy
                          - glyph_pos;
            if (max(max(points_01.x, points_01.z), point_2.x) * glyph_units_per_pixel.x < -0.5) break;
            uint root_mask = calcRootCode(points_01.y, points_01.w, point_2.y);
            if (root_mask != 0u) {
                vec2 crossings = solveHoriz(points_01, point_2) * glyph_units_per_pixel.x;
                if ((root_mask & 1u) != 0u) {
                    h_winding += clamp(crossings.x + 0.5, 0.0, 1.0);
                    h_edge_weight = max(h_edge_weight, clamp(1.0 - abs(crossings.x) * 2.0, 0.0, 1.0));
                }
                if (root_mask > 1u) {
                    h_winding -= clamp(crossings.y + 0.5, 0.0, 1.0);
                    h_edge_weight = max(h_edge_weight, clamp(1.0 - abs(crossings.y) * 2.0, 0.0, 1.0));
                }
            }
        }
    }
    // Vertical bands (ray in +Y)
    float v_winding = 0.0;
    float v_edge_weight = 0.0;
    {
        uvec4 band_header = texelFetch(usampler2D(band_tex, point_sampler),
                                       ivec2(glyph_loc.x + max_band_y + 1 + band_index.x, glyph_loc.y), 0);
        int curve_count = int(band_header.x);
        ivec2 entry_list_start = bandLoc(glyph_loc, int(band_header.y));
        for (int i = 0; i < curve_count && i < 128; i++) {
            ivec2 entry_coord = bandLoc(entry_list_start, i);
            uvec4 band_entry = texelFetch(usampler2D(band_tex, point_sampler), entry_coord, 0);
            ivec2 curve_tex_coord = ivec2(band_entry.x, band_entry.y);
            vec4 points_01 = texelFetch(sampler2D(curve_tex, point_sampler), curve_tex_coord, 0)
                            - vec4(glyph_pos, glyph_pos);
            ivec2 next_tex_coord = bandLoc(curve_tex_coord, 1);
            vec2 point_2 = texelFetch(sampler2D(curve_tex, point_sampler), next_tex_coord, 0).xy
                          - glyph_pos;
            if (max(max(points_01.y, points_01.w), point_2.y) * glyph_units_per_pixel.y < -0.5) break;
            uint root_mask = calcRootCode(points_01.x, points_01.z, point_2.x);
            if (root_mask != 0u) {
                vec2 crossings = solveVert(points_01, point_2) * glyph_units_per_pixel.y;
                if ((root_mask & 1u) != 0u) {
                    v_winding -= clamp(crossings.x + 0.5, 0.0, 1.0);
                    v_edge_weight = max(v_edge_weight, clamp(1.0 - abs(crossings.x) * 2.0, 0.0, 1.0));
                }
                if (root_mask > 1u) {
                    v_winding += clamp(crossings.y + 0.5, 0.0, 1.0);
                    v_edge_weight = max(v_edge_weight, clamp(1.0 - abs(crossings.y) * 2.0, 0.0, 1.0));
                }
            }
        }
    }
    float coverage = max(
        abs(h_winding * h_edge_weight + v_winding * v_edge_weight)
            / max(h_edge_weight + v_edge_weight, 1.0 / 65536.0),
        min(abs(h_winding), abs(v_winding))
    );
    coverage = clamp(coverage, 0.0, 1.0);
    float alpha = text_color.a * coverage;
    frag_color = vec4(text_color.rgb * alpha, alpha);
}
@end

@program slug vs fs
