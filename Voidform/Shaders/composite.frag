#version 150
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform vec2  uRes;
uniform float uTime;
uniform float uBloomAmt, uGrain, uChroma, uGlitch, uVignette, uExposure;
uniform float uCrt;      // 0..1 amount of old-monitor look: curved glass, scanlines, phosphor mask, falloff, flicker
uniform sampler2D uFrame;   // monitor frame (premultiplied RGBA), transparent over the screen
uniform vec4  uOpen;        // screen opening inside the window, uv (x0, y0, x1, y1), y up
uniform float uFrameOn;
uniform float uInk;      // 1 = ink on paper
uniform float uBg;       // background / paper level
uniform vec3  uAbsorb;   // ink absorption per channel (ink mode)

float h21(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec3 S(vec2 uv)
{
    return texture(uScene, uv).rgb + texture(uBloom, uv).rgb * uBloomAmt;
}

void main()
{
    // scene coordinates: 0..1 across the frame's screen opening
    vec2 su = (vUV - uOpen.xy) / (uOpen.zw - uOpen.xy);
    bool inside = su.x >= 0.0 && su.x <= 1.0 && su.y >= 0.0 && su.y <= 1.0;
    vec2 uv = su;
    float crtMask = 1.0;
    if (uCrt > 0.001)
    {
        vec2 cc = su * 2.0 - 1.0;
        cc *= 1.0 + 0.06 * uCrt * dot(cc, cc);                 // barrel distortion
        uv = cc * 0.5 + 0.5;
        float rr = 0.07 * uCrt;                                 // rounded glass corners
        vec2 q = abs(cc) - vec2(1.0 - rr);
        crtMask = 1.0 - smoothstep(-0.004, 0.012, length(max(q, 0.0)) - rr);
    }

    // ---- glitch: thin row tears, block tears, horizontal smears ----
    float rows  = floor(uRes.y * 0.5);
    float row   = floor(uv.y * rows);
    float tick  = floor(uTime * 14.0);
    float gA    = step(1.0 - 0.05 * uGlitch, h21(vec2(row, tick)));
    uv.x += (h21(vec2(row * 1.7, tick)) - 0.5) * 0.10 * uGlitch * gA;
    float blk   = floor(uv.y * 14.0);
    float bA    = step(1.0 - 0.07 * uGlitch, h21(vec2(blk + 40.0, floor(uTime * 6.0))));
    uv.x += (h21(vec2(blk, floor(uTime * 6.0))) - 0.5) * 0.06 * uGlitch * bA;

    // ---- chromatic aberration ----
    vec2 dir = uv - 0.5;
    float cm = uChroma * 0.014 * (0.25 + length(dir) * 2.0);
    vec3 c;
    c.r = S(uv + dir * cm).r;
    c.g = S(uv).g;
    c.b = S(uv - dir * cm).b;

    float sA = step(1.0 - 0.03 * uGlitch, h21(vec2(row + 91.0, floor(uTime * 8.0))));
    if (sA > 0.5)
    {
        vec3 st = vec3(0.0);
        for (int i = 1; i <= 14; ++i)
        {
            float f = float(i) / 14.0;
            st = max(st, texture(uScene, uv - vec2(f * 0.38, 0.0)).rgb * (1.0 - f));
        }
        c = max(c, st * 0.9);
    }

    c *= uExposure;

    vec3 col;
    float g  = h21(gl_FragCoord.xy + fract(uTime * 7.31) * vec2(131.0, 271.0));
    float g2 = h21(gl_FragCoord.yx * 1.37 + fract(uTime * 3.7) * 91.0);

    if (uInk > 0.5)
    {
        float dns = c.r;
        col = vec3(uBg) * exp(-dns * uAbsorb * 2.4);
        col *= 1.0 + (g - 0.5) * uGrain * 0.22;
        col *= 1.0 - uGrain * 0.35 * smoothstep(0.985, 1.0, g2);          // dark specks on the paper
        col  = mix(col, col * (1.0 - 0.18 * pow(length(su - 0.5) * 1.4, 2.0)), uVignette);
    }
    else
    {
        col = vec3(uBg) + (vec3(1.0) - exp(-c));
        float lum = dot(col, vec3(0.3333));
        col += uGrain * ((g - 0.5) * 0.24 * (0.25 + lum) + g2 * g2 * 0.05);
        col *= 1.0 - uVignette * pow(length(su - 0.5) * 1.4, 2.0);
    }
    if (uCrt > 0.001)
    {
        float row = su.y * uRes.y;
        col *= 1.0 - 0.16 * uCrt * (0.5 + 0.5 * sin(row * 3.14159));                 // scanlines
        float px = floor(gl_FragCoord.x);
        vec3 mask = vec3(1.0) - 0.10 * uCrt * vec3(mod(px, 3.0) != 0.0 ? 1.0 : 0.0,
                                                    mod(px + 1.0, 3.0) != 0.0 ? 1.0 : 0.0,
                                                    mod(px + 2.0, 3.0) != 0.0 ? 1.0 : 0.0);   // phosphor triads
        col *= mask * 1.08;
        col *= 1.0 - 0.28 * uCrt * pow(length(su - 0.5) * 1.35, 3.0);              // tube falloff
        col *= 1.0 + 0.03 * uCrt * sin(uTime * 55.0);                                // faint flicker
        col *= crtMask;
    }
    col = clamp(col, 0.0, 1.0);
    if (uFrameOn > 0.5)
    {
        vec4 fr = texture(uFrame, vec2(vUV.x, 1.0 - vUV.y));       // premultiplied
        col = (inside ? col * (1.0 - fr.a) : vec3(0.0) * (1.0 - fr.a)) + fr.rgb;
    }
    fragColor = vec4(col, 1.0);
}
