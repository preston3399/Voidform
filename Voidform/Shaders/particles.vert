#version 150
// Stateless GPU particle cloud: every particle is a pure function of gl_VertexID,
// time and the uniforms, so there are no buffers to upload and the count is just a draw call size.

uniform vec2  uRes;
uniform int   uMode;          // 0 = body points, 1 = filament lines (2 verts each)
uniform float uFlow;          // accumulated noise-flow phase
uniform float uRotY, uRotX, uZoom;
uniform float uSize, uWarp, uDetail, uMesh, uFuzz;
uniform float uFilLen, uWind;
uniform float uPunch;         // transient impulse (0..~2): explosion + shockwave strength
uniform float uWave;          // shockwave position 0..1.4 along the vertical axis
uniform float uMetal, uReflect;
uniform float uPerParticle;   // brightness of one particle (auto-normalised on the CPU)
uniform float uColorMix, uColorShift;
uniform float uInk;           // 1 = ink-on-paper mode (writes density instead of colour)
uniform vec3  uColA, uColB;
uniform int   uShape;         // 0 sphere 1 torus 2 galaxy 3 tunnel 4 terrain 5 explosion 6 urchin 7 shards 8 lattice 9 helix 10 star
uniform int   uStyle;         // 0 dots 1 streaks 2 shards 3 embers 4 pixels

out vec4 vColor;
flat out float vAng;
flat out float vStretch;

uint pcg(uint v)
{
    uint s = v * 747796405u + 2891336009u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint s) { s = pcg(s); return float(s) * (1.0 / 4294967295.0); }

float hash31(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float vnoise(vec3 x)
{
    vec3 i = floor(x), f = fract(x);
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float a = mix(mix(hash31(i + vec3(0,0,0)), hash31(i + vec3(1,0,0)), f.x),
                  mix(hash31(i + vec3(0,1,0)), hash31(i + vec3(1,1,0)), f.x), f.y);
    float b = mix(mix(hash31(i + vec3(0,0,1)), hash31(i + vec3(1,0,1)), f.x),
                  mix(hash31(i + vec3(0,1,1)), hash31(i + vec3(1,1,1)), f.x), f.y);
    return mix(a, b, f.z) * 2.0 - 1.0;
}
float fbm(vec3 p)
{
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 3; ++i) { s += a * vnoise(p); p = p * 2.03 + 11.7; a *= 0.5; }
    return s * 1.25;
}
vec3 rotY(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z); }
vec3 rotX(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z); }
vec3 rotAll(vec3 p) { return rotX(rotY(p, uRotY), uRotX); }

const float TAU = 6.2831853;

void main()
{
    bool lines = (uMode == 1);
    bool tip   = lines && ((gl_VertexID & 1) == 1);
    uint id    = lines ? uint(gl_VertexID >> 1) : uint(gl_VertexID);
    uint s     = id * 2u + 1013904223u + (lines ? 2654435769u : 0u);

    float r1 = rnd(s), r2 = rnd(s), r3 = rnd(s), r4 = rnd(s), r5 = rnd(s);
    float r6 = rnd(s), r7 = rnd(s), r8 = rnd(s), r9 = rnd(s);
    int sh = uShape;

    // ---- unit sphere direction (optionally snapped to lat/long grid lines) ----
    float z  = r2 * 2.0 - 1.0;
    float th = r1 * TAU;
    if (sh == 0 && r3 < uMesh)
    {
        const float NG = 46.0;
        if (r4 < 0.5) z  = (floor((z * 0.5 + 0.5) * NG) + 0.5) / NG * 2.0 - 1.0;
        else          th = (floor(r1 * NG * 1.5) + 0.5) / (NG * 1.5) * TAU;
    }
    float rxy = sqrt(max(1.0 - z * z, 0.0));
    vec3 d = vec3(rxy * cos(th), z, rxy * sin(th));

    // ---- base shape: position + outward normal ----
    vec3  base = d, nrm = d;
    float life = 1.0;       // per-particle alpha (explosion / tunnel fade)
    float bcell = r6;       // per-particle (or per-cell) randomness used by the beat explosion
    float wc = d.y * 0.5 + 0.5;   // coordinate the shockwave ring travels along

    if (sh == 1)            // torus
    {
        float u = r1 * TAU, v = r2 * TAU;
        if (r3 < uMesh)
        {
            if (r4 < 0.5) u = (floor(r1 * 64.0) + 0.5) / 64.0 * TAU;
            else          v = (floor(r2 * 24.0) + 0.5) / 24.0 * TAU;
        }
        nrm  = vec3(cos(v) * cos(u), sin(v), cos(v) * sin(u));
        base = 0.68 * vec3(cos(u), 0.0, sin(u)) + 0.32 * nrm;
    }
    else if (sh == 2)       // spiral galaxy
    {
        float rg = pow(r2, 0.65) * 1.25;
        if (r4 < 0.2)
        {
            base = normalize(vec3(r6 - 0.5, r7 - 0.5, r8 - 0.5) + 1e-4) * 0.3 * r5;
        }
        else
        {
            float arm = floor(r3 * 3.0) * TAU / 3.0;
            float a = arm + rg * 3.2 + uFlow * 0.35 / (0.25 + rg) + (r1 - 0.5) * (1.1 / (0.5 + rg * 2.5));
            base = vec3(rg * cos(a), (r7 - 0.5) * 0.2 * exp(-rg * 1.4), rg * sin(a));
        }
        nrm = normalize(vec3(0.0, 1.0, 0.0) + base * 0.4);
        wc = length(base) / 1.25;
    }
    else if (sh == 3)       // tunnel
    {
        float f = fract(r2 + uFlow * 0.12);
        float zc = (f * 2.0 - 1.0) * 3.0;
        if (r3 < uMesh) zc = (floor((zc + 3.0) / 0.35) + 0.5) * 0.35 - 3.0;
        float ang = r1 * TAU + uRotY;
        float R = 0.6 + 0.12 * sin(zc * 2.0 + uFlow);
        base = vec3(R * cos(ang), R * sin(ang), zc);
        nrm  = vec3(-cos(ang), -sin(ang), 0.0);
        life = smoothstep(0.0, 0.12, f) * smoothstep(1.0, 0.88, f);
        wc = f;
    }
    else if (sh == 4)       // wave terrain
    {
        float x = (r1 * 2.0 - 1.0) * 1.4, zz = (r2 * 2.0 - 1.0) * 1.4;
        if (r3 < uMesh)
        {
            if (r4 < 0.5) x  = (floor((x / 1.4 * 0.5 + 0.5) * 34.0) + 0.5) / 34.0 * 2.8 - 1.4;
            else          zz = (floor((zz / 1.4 * 0.5 + 0.5) * 34.0) + 0.5) / 34.0 * 2.8 - 1.4;
        }
        float rr = length(vec2(x, zz));
        base = vec3(x, 0.2 * sin(rr * 7.0 - uFlow * 2.0) * exp(-rr * 0.5) - 0.15, zz);
        nrm  = vec3(0.0, 1.0, 0.0);
        wc = rr / 1.4;
    }
    else if (sh == 5)       // explosion / fountain
    {
        float ph = fract(r1 + uFlow * 0.18 * (0.5 + r2 * 0.8));
        float sp = 0.4 + 1.3 * r3;
        base = d * sp * ph * 1.1;
        base.y -= 0.7 * ph * ph;
        nrm  = d;
        life = pow(1.0 - ph, 1.5);
        wc = ph;
    }
    else if (sh == 6)       // crystal urchin: a small core with long spikes
    {
        if (r4 < 0.65)
        {
            uint ss = uint(floor(r3 * 18.0)) * 977u + 31u;
            vec3 sd = vec3(rnd(ss), rnd(ss), rnd(ss)) * 2.0 - 1.0;
            sd = normalize(sd + 1e-4);
            float L = (0.6 + rnd(ss) * 0.9) * (1.0 + uWarp * 0.4 + uPunch * 0.5);
            float t = r2;
            vec3 dir = normalize(sd + (d - sd * dot(d, sd)) * (1.0 - t) * 0.14);
            base = dir * (0.4 + L * t);
            nrm  = dir;
        }
        else
        {
            base = d * 0.42;
        }
        wc = length(base) / 1.5;
    }
    else if (sh == 7)       // faceted shards (stepped octahedron)
    {
        vec3 o = d / (abs(d.x) + abs(d.y) + abs(d.z));
        float h = hash31(floor(o * 4.0 + 0.5) + vec3(7.0));
        float ext = 0.9 + (h - 0.5) * 0.5 * (0.3 + uWarp);
        if (h > 0.86) ext *= 1.7;
        base = o * ext * 1.15;
        nrm  = normalize(sign(d) + 0.001);
        bcell = h;
    }
    else if (sh == 8)       // lattice cube
    {
        int ax = int(floor(r3 * 3.0));
        float sg = (r4 < 0.5) ? -1.0 : 1.0;
        vec2 uv = vec2(r1, r2) * 2.0 - 1.0;
        if (r5 < 0.92)
        {
            const float NL = 10.0;
            if (r6 < 0.5) uv.x = (floor((uv.x * 0.5 + 0.5) * NL + 0.5) / NL) * 2.0 - 1.0;
            else          uv.y = (floor((uv.y * 0.5 + 0.5) * NL + 0.5) / NL) * 2.0 - 1.0;
        }
        if (ax == 0)      { base = vec3(sg, uv.x, uv.y); nrm = vec3(sg, 0.0, 0.0); }
        else if (ax == 1) { base = vec3(uv.x, sg, uv.y); nrm = vec3(0.0, sg, 0.0); }
        else              { base = vec3(uv.x, uv.y, sg); nrm = vec3(0.0, 0.0, sg); }
        base *= 0.78;
        bcell = hash31(floor(base * 5.0 + 0.5) + vec3(3.0));
    }
    else if (sh == 9)       // DNA helix
    {
        float t = (r1 * 2.0 - 1.0) * 1.15;
        float k = r3;
        const float R = 0.42;
        if (k < 0.8)
        {
            float a = t * 3.6 + uFlow * 0.5 + (k < 0.4 ? 0.0 : 3.14159);
            base = vec3(R * cos(a), t, R * sin(a)) + (vec3(r6, r7, r8) - 0.5) * 0.05;
            nrm  = vec3(cos(a), 0.0, sin(a));
        }
        else
        {
            float tq = floor(t * 9.0) / 9.0;
            float aq = tq * 3.6 + uFlow * 0.5;
            vec3 p0 = vec3(R * cos(aq), tq, R * sin(aq));
            vec3 p1 = vec3(-p0.x, tq, -p0.z);
            base = mix(p0, p1, r2);
            nrm  = vec3(cos(aq), 0.0, sin(aq));
        }
        wc = t * 0.5 / 1.15 + 0.5;
    }
    else if (sh == 10)      // star burst: layered jagged star outlines
    {
        float K = floor(3.0 + uDetail * 2.5);
        float l = floor(r3 * 5.0);
        float e = r1 * 2.0 * K;
        float i0 = floor(e), f = fract(e);
        float Ro = 1.0 + 0.15 * uWarp, Ri = 0.32;
        float a0 = i0 * 3.14159265 / K + l * 0.22, a1 = (i0 + 1.0) * 3.14159265 / K + l * 0.22;
        vec2 v0 = vec2(cos(a0), sin(a0)) * ((mod(i0, 2.0) < 0.5) ? Ro : Ri);
        vec2 v1 = vec2(cos(a1), sin(a1)) * ((mod(i0, 2.0) < 0.5) ? Ri : Ro);
        vec2 pxz = mix(v0, v1, f) * (1.0 - 0.14 * l);
        base = vec3(pxz.x, (l - 2.0) * 0.09 + (r7 - 0.5) * 0.03, pxz.y);
        nrm  = normalize(vec3(pxz.x, 0.15, pxz.y) + 1e-4);
        wc = length(pxz);
    }

    // ---- displacement field ----
    vec3 q = base * uDetail + vec3(0.0, uFlow * 0.6, uFlow);
    vec3 w = vec3(vnoise(q * 1.3 + vec3(1.7, 9.2, 3.1)),
                  vnoise(q * 1.3 + vec3(8.3, 2.8, 5.5)),
                  vnoise(q * 1.3 + vec3(4.1, 6.6, 0.9)));
    float n = fbm(q + w * 0.9);
    if (sh == 7 || sh == 8) n = 0.0;     // keep the sharp shapes sharp: no smooth blobbing
    float ring  = exp(-pow((wc - uWave) * 6.0, 2.0)) * uPunch;
    float burst = uPunch * (0.1 + 0.9 * bcell) * 0.6;
    float disp  = uWarp * n + ring * 0.55 + burst;
    // thin shapes (torus, galaxy, tunnel, helix...) must not be blobbed out of recognition
    float dk = (sh == 0) ? 1.0 : (sh == 5 || sh == 6) ? 0.6 : (sh == 7 || sh == 8) ? 0.8 : 0.22;
    vec3 pos = base + nrm * disp * dk;
    pos += (vec3(r6, r7, r8) - 0.5) * uFuzz;
    vec3 N3 = normalize(nrm + uWarp * 1.1 * w);

    if (lines)
    {
        float len = uFilLen * (0.08 + r9 * r9 * r9 * 1.5) * (1.0 + uPunch * 0.7);
        vec3 dirOut = normalize(nrm + 0.6 * w + vec3(0.0, uWind, 0.0));
        if (tip) pos += dirOut * len;
    }

    vec3 P, Nr;
    if (sh == 3)
    {
        P  = rotX(pos, uRotX * 0.3);
        Nr = rotX(N3, uRotX * 0.3);
    }
    else
    {
        float tiltOff = (sh == 1) ? 0.9 : (sh == 2) ? 0.8 : (sh == 4) ? 0.5 : (sh == 10) ? 0.8 : 0.0;
        P  = rotX(rotY(pos, uRotY), uRotX + tiltOff);
        Nr = rotX(rotY(N3, uRotY), uRotX + tiltOff);
    }

    const float camZ = 3.4;
    float persp = camZ / max(camZ - P.z, 1.4);   // clamp: stuff near the camera must not blow up
    vec2 ndc = P.xy * persp * 0.42 * uZoom;
    ndc.x *= uRes.y / uRes.x;

    // ---- particle style: size / stretch / orientation ----
    float sz = uSize * (uRes.y / 1080.0) * persp * (0.7 + 0.6 * r5);
    float stretch = 1.0;
    float styleGain = 1.0;
    vec2 sdir = vec2(ndc.x * uRes.x, ndc.y * uRes.y);
    vAng = atan(sdir.y, sdir.x);
    if (!lines)
    {
        if (uStyle == 1)        // streaks radiate away from the centre
        {
            stretch = 3.0 + 5.0 * min(uPunch, 1.3);
            styleGain = pow(stretch, -0.55);
        }
        else if (uStyle == 2)   // tumbling shards
        {
            sz *= 1.8;
            stretch = 2.0 + 2.5 * r8;
            vAng = r9 * TAU + uFlow * (0.4 + r7);
            styleGain = 1.6 / stretch;
        }
        else if (uStyle == 3)   // embers: sparse hot sparks
        {
            sz *= 0.6 + 0.9 * r7;
            styleGain = 0.25 + 2.6 * r8 * r8 * r8;
        }
        else if (uStyle == 4)   // pixels: snap to a coarse grid
        {
            float cell = max(2.0, uSize * 2.5 * (uRes.y / 1080.0));
            vec2 pxp = ndc * uRes * 0.5;
            pxp = (floor(pxp / cell) + 0.5) * cell;
            ndc = pxp * 2.0 / uRes;
            sz = cell;
            styleGain = 0.7;
        }
    }
    vStretch = stretch;
    gl_Position = vec4(ndc, 0.0, 1.0);
    float ps = sz * stretch;
    if (ps > 64.0) { stretch *= 64.0 / ps; vStretch = stretch; ps = 64.0; }
    gl_PointSize = lines ? clamp(sz, 1.0, 24.0) : clamp(ps, 1.0, 64.0);
    if (!lines && uStyle != 1 && uStyle != 2) gl_PointSize = clamp(sz, 1.0, 24.0);
    if (!lines && uStyle == 2) gl_PointSize = clamp(ps, 1.0, 40.0);

    // ---- shading: tint + diffuse + procedural studio reflection ----
    float t = clamp(0.5 + n * 0.9 + uColorShift + (r5 - 0.5) * 0.15 + (sh == 7 || sh == 8 ? (bcell - 0.5) * 0.6 : 0.0), 0.0, 1.0);
    vec3 tint = mix(uColA, uColB, t * uColorMix);

    vec3 V = vec3(0.0, 0.0, 1.0);
    float ndv = clamp(dot(Nr, V), -1.0, 1.0);
    float dif = max(dot(Nr, normalize(vec3(-0.5, 0.7, 0.55))), 0.0);
    vec3 R = reflect(-V, Nr);
    float top  = smoothstep(0.15, 0.95, R.y);
    float side = smoothstep(0.55, 0.98, dot(R, normalize(vec3(0.8, 0.15, 0.5))));
    float hor  = pow(max(1.0 - abs(R.y), 0.0), 6.0) * 0.25;
    float env  = 0.06 + top * 1.2 + side * 0.9 + hor;
    float fres = pow(max(1.0 - abs(ndv), 0.0), 3.0);

    vec3 specCol = mix(vec3(1.0), tint, uMetal);
    vec3 diffuse = tint * (0.5 + 0.7 * dif) * (1.0 - 0.9 * uMetal);
    vec3 spec    = specCol * (env * mix(0.15, 1.0, uMetal) + fres * 0.6) * uReflect;
    vec3 col     = diffuse + spec;

    float facing = 0.3 + 0.7 * smoothstep(-0.6, 0.6, ndv);
    facing = mix(facing, 0.75 + 0.25 * facing, uInk);
    float fade = lines ? (tip ? 0.0 : 0.9) : 1.0;
    float a = uPerParticle * facing * fade * life * styleGain;
    if (uStyle == 3 && !lines) col = mix(col, col * vec3(1.6, 1.0, 0.55), 0.5);

    float rim = 0.55 + 1.5 * pow(max(1.0 - abs(ndv), 0.0), 2.0);
    vec3 inkCol = vec3(a * rim);
    vColor = vec4(mix(col * a, inkCol, uInk), 1.0);
}
