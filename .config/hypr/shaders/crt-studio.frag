#version 300 es
// crt-studio.frag — Braun / P4 White-Phosphor Broadcast Monitor Shader
precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
out vec4 fragColor;

void main() {
    // Subtle studio barrel curvature
    vec2 centered = v_texcoord * 2.0 - 1.0;
    vec2 warped = centered * (1.0 + 0.018 * dot(centered, centered));
    vec2 uv = warped * 0.5 + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 pix = texture(tex, uv);
    float luma = dot(pix.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Fine P4 phosphor scanlines (subtle enough to keep 10pt code razor-readable)
    float scan = 0.94 + 0.06 * sin(uv.y * 1080.0 * 3.14159265);
    vec3 p4 = vec3(luma * 0.99, luma * 1.00, min(1.0, luma * 1.02)) * scan;
    fragColor = vec4(clamp(p4, 0.0, 1.0), pix.a);
}
