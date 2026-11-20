#version 150
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uPrev;
uniform float uDecay;
void main()
{
    vec4 c = texture(uPrev, vUV);
    // sanitise: a single NaN/inf pixel would otherwise live forever in the feedback loop
    if (any(isnan(c)) || any(isinf(c))) c = vec4(0.0);
    fragColor = min(c, vec4(64.0)) * uDecay;
}
