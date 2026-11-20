#version 150
// Dual-Kawase downsample. uHalfPixel = 0.5 / destination size.
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTex;
uniform vec2 uHalfPixel;
void main()
{
    vec2 h = uHalfPixel;
    vec4 s = texture(uTex, vUV) * 4.0;
    s += texture(uTex, vUV + vec2(-h.x, -h.y));
    s += texture(uTex, vUV + vec2( h.x, -h.y));
    s += texture(uTex, vUV + vec2(-h.x,  h.y));
    s += texture(uTex, vUV + vec2( h.x,  h.y));
    s *= 0.125;
    if (any(isnan(s)) || any(isinf(s))) s = vec4(0.0);
    fragColor = s;
}
