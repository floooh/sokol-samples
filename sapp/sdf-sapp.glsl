//------------------------------------------------------------------------------
//  Signed-distance-field raymarching shaders, see:
//  http://www.iquilezles.org/default.html
//------------------------------------------------------------------------------

//--- vertex shader
@vs vs
uniform vs_params {
    float time;
};
in vec4 position;

out vec2 pos;
out vec3 eye;
out vec3 up;
out vec3 right;
out vec3 fwd;

// compute eye position (orbit around center)
vec3 eye_pos(float time, vec3 center) {
    return center + vec3(sin(time * 0.1) * 5.0, 2.0, cos(time * 0.1) * 5.0);
}

// a lookat function
void lookat(vec3 eye, vec3 center, vec3 up, out vec3 out_fwd, out vec3 out_right, out vec3 out_up) {
    out_fwd = normalize(center - eye);
    out_right = normalize(cross(out_fwd, up));
    out_up = cross(out_right, out_fwd);
}

void main() {
    gl_Position = position;
    pos = position.xy;
    vec3 center = vec3(0.0, 0.0, 0.0);
    vec3 up_vec = vec3(0.0, 1.0, 0.0);
    eye = eye_pos(time * 5, center);
    lookat(eye, center, vec3(0.0, 1.0, 0.0), fwd, right, up);
}
@end

//--- fragment shader
@fs fs
in vec2 pos;
in vec3 eye;
in vec3 up;
in vec3 right;
in vec3 fwd;

out vec4 frag_color;

// distance-field function for box
float sd_box(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min( max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

// distance-field function for the whole scene
float d_scene(vec3 p) {
    return sd_box(p, vec3(1.0, 1.0, 1.0));
}

// surface normal estimation
vec3 surface_normal(vec3 p) {
    const float eps = 0.01;
    const vec3 dx = vec3(eps, 0, 0);
    const vec3 dy = vec3(0, eps, 0);
    const vec3 dz = vec3(0, 0, eps);
    float x = d_scene(p + dx) - d_scene(p - dx);
    float y = d_scene(p + dy) - d_scene(p - dy);
    float z = d_scene(p + dz) - d_scene(p - dz);
    return normalize(vec3(x, y, z));
}

void main() {
    const float epsilon = 0.001;
    const float focal_length = 2.0;

    vec3 ray_origin = eye + fwd * focal_length + right * pos.x + up * pos.y;
    vec3 ray_direction = normalize(ray_origin - eye);

    vec4 color = vec4(0.5, 0.6, 0.7, 1.0);
    float t = 0.0;
    for (int i = 0; i < 96; i++) {
        vec3 p = ray_origin + ray_direction * t;
        float d = d_scene(p);
        if (d < epsilon) {
            vec3 nrm = surface_normal(p) * 0.5 + 0.5;
            color = vec4(nrm, 1.0);
            break;
        }
        t += d;
    }
    frag_color = color;
}
@end

@program sdf vs fs

