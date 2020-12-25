//------------------------------------------------------------------------------
//  float/rgba8 encoding/decoding so that we can use an RGBA8
//  shadow map instead of floating point render targets which might
//  not be supported everywhere
//
//  http://aras-p.info/blog/2009/07/30/encoding-floats-to-rgba-the-final/
//
#pragma sokol @ctype mat4 hmm_mat4
#pragma sokol @ctype vec3 hmm_vec3
#pragma sokol @ctype vec2 hmm_vec2
#pragma sokol @block util
vec4 encodeDepth(float v) {
    vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
    enc = fract(enc);
    enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
    return enc;
}

float decodeDepth(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));
}

//------------------------------------------------------------------------------
//  perform simple shadow map lookup returns 0.0 (unlit) or 1.0 (lit)
//
float sampleShadow(sampler2D shadowMap, vec2 uv, float compare) {
    #if !SOKOL_GLSL
    uv.y = 1.0-uv.y;
    #endif
    float depth = decodeDepth(texture(shadowMap, vec2(uv.x, uv.y)));
    depth += 0.001;
    return step(compare, depth);
}

//------------------------------------------------------------------------------
//  perform percentage-closer shadow map lookup
//
float sampleShadowPCF(sampler2D shadowMap, vec2 uv, vec2 smSize, float compare) {
    float result = 0.0;
    for (int x=-2; x<=2; x++) {
        for (int y=-2; y<=2; y++) {
            vec2 off = vec2(x,y)/smSize;
            result += sampleShadow(shadowMap, uv+off, compare);
        }
    }
    return result / 25.0;
}

//------------------------------------------------------------------------------
//  perform gamma correction
//
vec4 gamma(vec4 c) {
    float p = 1.0/2.2;
    return vec4(pow(c.xyz, vec3(p, p, p)), c.w);
}
#pragma sokol @end

#pragma sokol @block uniforms
uniform vs_shadow_params {
    mat4 mvp;
};
#pragma sokol @end
//------------------------------------------------------------------------------
//  Shadowmap pass shaders
//
#pragma sokol @vs shadowVS
#pragma sokol @include_block uniforms

in vec4 position;
out vec2 projZW;

void main() {
    gl_Position = mvp * position;
    projZW = gl_Position.zw;
}
#pragma sokol @end

#pragma sokol @fs shadowFS
#pragma sokol @include_block util
in vec2 projZW;
out vec4 fragColor;

void main() {
    float depth = projZW.x / projZW.y;
    fragColor = encodeDepth(depth);
}
#pragma sokol @end

#pragma sokol @program shadow shadowVS shadowFS

//------------------------------------------------------------------------------
//  Color pass shaders
//
#pragma sokol @block colorUniforms
uniform vs_light_params {
    mat4 model;
    mat4 mvp;
    mat4 lightMVP;
    vec3 diffColor;
};
#pragma sokol @end

#pragma sokol @vs colorVS
#pragma sokol @include_block colorUniforms
in vec4 position;
in vec3 normal;
out vec3 color;
out vec4 lightProjPos;
out vec4 P;
out vec3 N;

void main() {
    gl_Position = mvp * position;
    lightProjPos = lightMVP * position;
    P = (model * position);
    N = (model * vec4(normal, 0.0)).xyz;
    color = diffColor;
}
#pragma sokol @end

#pragma sokol @fs colorFS
#pragma sokol @include_block util

uniform fs_light_params {
    vec2 shadowMapSize;
    vec3 lightDir;
    vec3 eyePos;
};
uniform sampler2D shadowMap;

in vec3 color;
in vec4 lightProjPos;
in vec4 P;
in vec3 N;
out vec4 fragColor;

void main() {
    float specPower = 2.2;
    float ambientIntensity = 0.25;

    // diffuse lighting
    vec3 l = normalize(lightDir);
    vec3 n = normalize(N);
    float n_dot_l = dot(n,l);
    if (n_dot_l > 0.0) {

        vec3 lightPos = lightProjPos.xyz / lightProjPos.w;
        vec2 smUV = (lightPos.xy+1.0)*0.5;
        float depth = lightPos.z;
        float s = sampleShadowPCF(shadowMap, smUV, shadowMapSize, depth);

        float diffIntensity = max(n_dot_l * s, 0.0);

        vec3 v = normalize(eyePos - P.xyz);
        vec3 r = reflect(-l, n);
        float r_dot_v = max(dot(r, v), 0.0);
        float specIntensity = pow(r_dot_v, specPower) * n_dot_l * s;

        fragColor = vec4(vec3(specIntensity, specIntensity, specIntensity) + (diffIntensity+ambientIntensity)*color, 1.0);
    }
    else {
        fragColor = vec4(color * ambientIntensity, 1.0);
    }
    fragColor = gamma(fragColor);
}
#pragma sokol @end

#pragma sokol @program color colorVS colorFS
