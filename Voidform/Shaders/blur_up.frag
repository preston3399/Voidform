#version 150
// Dual-Kawase upsample, blended additively onto the larger level.
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTex;
uniform vec2 uHalfPixel;   // 0.5 / source (smaller) size
uniform float uWeight;
void main()
{
    vec2 h = uHalfPixel;
    vec4 s = texture(uTex, vUV + vec2(-h.x * 2.0, 0.0));
    s += texture(uTex, vUV + vec2(-h.x,  h.y)) * 2.0;
    s += texture(uTex, vUV + vec2(0.0,  h.y * 2.0));
    s += texture(uTex, vUV + vec2( h.x,  h.y)) * 2.0;
    s += texture(uTex, vUV + vec2( h.x * 2.0, 0.0));
    s += texture(uTex, vUV + vec2( h.x, -h.y)) * 2.0;
    s += texture(uTex, vUV + vec2(0.0, -h.y * 2.0));
    s += texture(uTex, vUV + vec2(-h.x, -h.y)) * 2.0;
    fragColor = s * (uWeight / 12.0);
}
