@vs vs

// NOTE: mixing float and int base types in one uniform block is fairly
// inefficient in the sokol-gfx GL backend, because the uniform
// block will not be flattened into a single float or int array!
//
uniform vs_params {
    vec2 offset;
    vec2 scale;
    // scramble the vector sizes to enforce 'weird' padding
    int sel;
    ivec2 i2;
    int i1;
    ivec4 i4;
    vec4 pal[10];
    ivec3 i3;
};

in vec2 position;
out vec4 color;

void main() {
    gl_Position = vec4(position * scale + offset, 0.0, 1.0);
    if (sel == 0) {
        color = pal[i1];
    }
    else if (sel == 1) {
        color = pal[i2.x];
    }
    else if (sel == 2) {
        color = pal[i2.y];
    }
    else if (sel == 3) {
        color = pal[i3.x];
    }
    else if (sel == 4) {
        color = pal[i3.y];
    }
    else if (sel == 5) {
        color = pal[i3.z];
    }
    else if (sel == 6) {
        color = pal[i4.x];
    }
    else if (sel == 7) {
        color = pal[i4.y];
    }
    else if (sel == 8) {
        color = pal[i4.z];
    }
    else if (sel == 9) {
        color = pal[i4.w];
    }
    else {
        color = vec4(1.0, 1.0, 1.0, 1.0);
    }
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program uniformtypes vs fs

