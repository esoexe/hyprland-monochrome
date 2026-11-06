#version 300 es
// paper-eink.frag — Matte High-Readability E-Ink Paper Tone
precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
out vec4 fragColor;

void main() {
    vec4 pix = texture(tex, v_texcoord);
    float luma = dot(pix.rgb, vec3(0.2126, 0.7152, 0.0722));
    // Lift harsh #000000 to soft matte ink (#121212) and cap peak white glare (#ebebeb)
    float matte = mix(0.07, 0.92, smoothstep(0.0, 1.0, luma));
    fragColor = vec4(vec3(matte), pix.a);
}
