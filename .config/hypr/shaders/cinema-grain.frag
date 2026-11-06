#version 300 es
// cinema-grain.frag — 35mm Silver-Halide Film Grain & Optical Vignette
precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
out vec4 fragColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec4 pix = texture(tex, v_texcoord);
    float luma = dot(pix.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 base = mix(pix.rgb, vec3(luma), 0.85);

    // Subtle corner falloff
    vec2 uv = v_texcoord - 0.5;
    float vig = 1.0 - dot(uv, uv) * 0.42;

    // Fine 35mm halide grain weighted toward midtones
    float grain = (hash12(v_texcoord * 2048.0) - 0.5) * 0.032 * (1.0 - abs(luma - 0.5));
    vec3 outRgb = clamp(base * vig + grain, 0.0, 1.0);
    fragColor = vec4(outRgb, pix.a);
}
