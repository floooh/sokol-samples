@ctype mat4 hmm_mat4

@block noise_utils
//
// Description : Array and textureless GLSL 2D simplex noise function.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : ijm
//     Lastmod : 20110822 (ijm)
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
//
vec3 mod289(vec3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 mod289(vec2 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 permute(vec3 x) {
    return mod289(((x*34.0)+1.0)*x);
}

float snoise(vec2 v)
{
    const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
    0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
    -0.577350269189626,  // -1.0 + 2.0 * C.x
    0.024390243902439); // 1.0 / 41.0
    // First corner
    vec2 i  = floor(v + dot(v, C.yy) );
    vec2 x0 = v -   i + dot(i, C.xx);

    // Other corners
    vec2 i1;
    //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
    //i1.y = 1.0 - i1.x;
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    // x0 = x0 - 0.0 + 0.0 * C.xx ;
    // x1 = x0 - i1 + 1.0 * C.xx ;
    // x2 = x0 - 1.0 + 2.0 * C.xx ;
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;

    // Permutations
    i = mod289(i); // Avoid truncation effects in permutation
    vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
    + i.x + vec3(0.0, i1.x, 1.0 ));

    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
    m = m*m ;
    m = m*m ;

    // Gradients: 41 points uniformly over a line, mapped onto a diamond.
    // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)

    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;

    // Normalise gradients implicitly by scaling m
    // Approximation of: m *= inversesqrt( a0*a0 + h*h );
    m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

    // Compute final noise value at P
    vec3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}
@end

// render plasma into a 'fullscreen triangle'
@vs vs_plasma

// fullscreen triangle vertex positions
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };

out vec2 uv;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    uv = (pos + 1) * 0.5;
}
@end

@fs fs_plasma
@include_block noise_utils

layout(binding=0) uniform plasma_params {
    float time;
};
in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 dx = vec2(time, 0.0);
    vec2 dy = vec2(0.0, time);
    vec2 dxy = vec2(time, time);

    float red;
    red  = (snoise((uv * 1.5) + dx) * 0.5) + 0.5;
    red += snoise((uv * 5.0) + dx) * 0.15;
    red += snoise((uv * 5.0) + dy) * 0.15;

    float green;
    green  = (snoise((uv * 1.5) + dy) * 0.5) + 0.5;
    green += snoise((uv * 5.0) + dy) * 0.15;
    green += snoise((uv * 5.0) + dx) * 0.15;

    float blue;
    blue  = (snoise((uv * 1.5) + dxy) * 0.5) + 0.5;
    blue += snoise((uv * 5.0) + dxy) * 0.15;
    blue += snoise((uv * 5.0) - dxy) * 0.15;

    float height = 0.0;
    height = (snoise((uv * 3.0) + dxy) * 0.5) + 0.5;
    height += snoise(uv * 20.0 + dy) * 0.1;
    height += snoise(uv * 20.0 - dy) * 0.1;

    frag_color = vec4(red, green, blue, height * 0.2);
}
@end

@program plasma vs_plasma fs_plasma

// shaders for rendering a vertex-displaced plane with vertices synthesized in shader
@vs vs_display
layout(binding=0) uniform vs_params {
    mat4 mvp;
};
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

out vec4 color;

void main() {
    // restore tile coords from vertex index
    ivec2 tile_xz = ivec2(gl_VertexIndex & 255, gl_VertexIndex >> 8);
    const float dxz = 2.0 / 256.0;

    // fetch plasma value computed in offscreen pass
    const vec4 plasma = texelFetch(sampler2D(tex, smp), tile_xz, 0);

    // compute vertex x/y position
    const vec4 pos = vec4(-1.0 + tile_xz.x * dxz, plasma.w, -1.0 + tile_xz.y * dxz, 1);
    gl_Position = mvp * pos;
    color = vec4(plasma.xyz, 1);
}
@end

@fs fs_display
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program display vs_display fs_display
