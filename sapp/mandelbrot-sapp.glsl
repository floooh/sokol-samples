@vs vs
out vec2 uv;

void main() {
    // synthesize of 'fullscreen triangle'
    float x = (gl_VertexIndex & 1) != 0 ? 2.0 : 0.0;
    float y = (gl_VertexIndex & 2) != 0 ? 2.0 : 0.0;
    gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.0, 1.0);
    uv = vec2(x, 1.0 - y);
}
@end

@fs fs
layout(binding=0) uniform fs_params {
    float time;
    float width;
    vec2 aspect;
    int iter_max;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 target = vec2(-0.7756838, 0.1364674);
    vec2 c = target + ((uv - 0.5) * width * aspect);
    vec2 z = vec2(0.0);
    float n = 0.0;
    for (int i = 0; i < iter_max; i++, n += 1.0) {
        z = vec2(z.x * z.x - z.y * z.y + c.x, 2.0 * z.x * z.y + c.y);
        if (dot(z, z) > 4.0) {
            break;
        }
    }
    float t = n / float(iter_max);
    float phase = time * 0.33;
    // rainbow-coloring via phase-shifted cosines
    float r = 0.0, g = 0.0, b = 0.0;
    if (n < float(iter_max)) {
        r = 0.5 + 0.5 * cos(6.28 * (t * 2.0 + phase));
        g = 0.5 + 0.5 * cos(6.28 * (t * 2.0 + phase + 0.33));
        b = 0.5 + 0.5 * cos(6.28 * (t * 2.0 + phase + 0.67));
    }
    frag_color = vec4(r, g, b, 1.0);
}
@end

@program mandelbrot vs fs
