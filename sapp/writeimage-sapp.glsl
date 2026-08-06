
// vertex shader for a bufferless 'fullscreen triangle'
@vs vs
out vec2 uv;
void main() {
    float x = (gl_VertexIndex & 1) != 0 ? 2.0 : 0.0;
    float y = (gl_VertexIndex & 2) != 0 ? 2.0 : 0.0;
    gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.5, 1.0);
    uv = vec2(x, 1.0 - y);
}
@end

@block fs_common
layout(binding=0) uniform fs_params {
    float miplevel;
    float slice;
};
layout(binding=0) uniform sampler smp;
in vec2 uv;
out vec4 frag_color;
@end

@fs fs_tex2d
@include_block fs_common
layout(binding=0) uniform texture2D tex2d;
void main() {
    frag_color = textureLod(sampler2D(tex2d, smp), uv, miplevel);
}
@end

@fs fs_texarray
@include_block fs_common
layout(binding=0) uniform texture2DArray texarray;
void main() {
    frag_color = textureLod(sampler2DArray(texarray, smp), vec3(uv, slice), miplevel);
}
@end

@fs fs_texcube
@include_block fs_common
layout(binding=0) uniform textureCube texcube;
void main() {
    vec2 t = uv * 2.0 - 1.0;
    vec3 dir;
    int face = int(slice);
    switch (face) {
        case 0:  dir = vec3( 1.0, -t.y, -t.x); break; // +X
        case 1:  dir = vec3(-1.0, -t.y,  t.x); break; // -X
        case 2:  dir = vec3( t.x,  1.0,  t.y); break; // +Y
        case 3:  dir = vec3( t.x, -1.0, -t.y); break; // -Y
        case 4:  dir = vec3( t.x, -t.y,  1.0); break; // +Z
        default: dir = vec3(-t.x, -t.y, -1.0); break; // -Z
    }
    frag_color = textureLod(samplerCube(texcube, smp), dir, miplevel);
}
@end

@program tex2d vs fs_tex2d
@program texarray vs fs_texarray
@program texcube vs fs_texcube
