#version 150
in vec4 vColor;
flat in float vAng;
flat in float vStretch;
out vec4 fragColor;
uniform int uMode;    // 0 = points, 1 = filament lines
uniform int uStyle;   // 0 dots 1 streaks 2 shards 3 embers 4 pixels
void main()
{
    float a = 1.0;
    if (uMode == 0)
    {
        vec2 c = (gl_PointCoord * 2.0 - 1.0) * vec2(1.0, -1.0);
        if (uStyle == 1 || uStyle == 2)
        {
            float cs = cos(vAng), sn = sin(vAng);
            vec2 p = vec2(cs * c.x + sn * c.y, -sn * c.x + cs * c.y);   // x along the streak / shard axis
            if (uStyle == 1)
            {
                float d = p.x * p.x + p.y * p.y * vStretch * vStretch;
                if (d > 1.0) discard;
                a = (1.0 - d); a *= a;
                a *= mix(0.35, 1.0, p.x * 0.5 + 0.5);   // bright head, fading tail
            }
            else
            {
                float d = abs(p.x) + abs(p.y) * vStretch * 0.6;    // thin diamond = sharp sliver
                if (d > 1.0) discard;
                a = mix(0.55, 1.0, 1.0 - d);
            }
        }
        else if (uStyle == 4)
        {
            if (max(abs(c.x), abs(c.y)) > 0.92) discard;
            a = 1.0;
        }
        else
        {
            float d = dot(c, c);
            if (d > 1.0) discard;
            a = 1.0 - d;
            a *= a;
            a = (uStyle == 3) ? a * a : mix(a, 1.0, 0.25);
        }
    }
    fragColor = vec4(vColor.rgb * a, 1.0);
}
