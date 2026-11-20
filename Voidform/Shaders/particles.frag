#version 150
in vec4 vColor;
out vec4 fragColor;
uniform int uMode;   // 0 = points, 1 = filament lines
void main()
{
    float a = 1.0;
    if (uMode == 0)
    {
        vec2 c = gl_PointCoord * 2.0 - 1.0;
        float d = dot(c, c);
        if (d > 1.0) discard;
        a = 1.0 - d;
        a *= a;
        a = mix(a, 1.0, 0.25);
    }
    fragColor = vec4(vColor.rgb * a, 1.0);
}
