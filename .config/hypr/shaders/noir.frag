#version 300 es
// noir.frag — Hardware Rec.709 Pure Monochrome Framebuffer Filter
// Converts every app, video, and browser surface to Leica-style silver greyscale.
precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
out vec4 fragColor;

void main() {
    vec4 pix = texture(tex, v_texcoord);
    float luma = dot(pix.rgb, vec3(0.2126, 0.7152, 0.0722));
    // Gentle S-curve micro-contrast so blacks stay deep and highlights gleam
    float curved = clamp(smoothstep(-0.02, 1.02, luma), 0.0, 1.0);
    float toned = mix(luma, curved, 0.28);
    fragColor = vec4(vec3(toned), pix.a);
}
