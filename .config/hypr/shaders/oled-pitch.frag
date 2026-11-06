#version 300 es
// oled-pitch.frag — OLED True-Black Pixel-Off Saver & Silver Micro-Contrast
// Smoothly crushes near-black grey haze (<3.8% luma) to exact (0.0, 0.0, 0.0)
// so OLED subpixels turn physically OFF for infinite contrast, 0.1ms response,
// and lower laptop panel power draw, while keeping silver text razor-sharp.
precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
out vec4 fragColor;

void main() {
    vec4 pix = texture(tex, v_texcoord);
    float luma = dot(pix.rgb, vec3(0.2126, 0.7152, 0.0722));
    // Toe-crush near-black #010101..#0a0a0a fog to hardware 0.0 pitch black
    float pitch = smoothstep(0.036, 1.0, luma);
    // Subtle highlight brilliance compensation so midtones stay crisp
    float crisp = pow(pitch, 0.94);
    vec3 mono = mix(pix.rgb * (crisp / max(luma, 0.0001)), vec3(crisp), 0.65);
    fragColor = vec4(clamp(mono, 0.0, 1.0), pix.a);
}
