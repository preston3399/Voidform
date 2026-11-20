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

out vec4 vColor;

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

void main()
{
    bool lines = (uMode == 1);
    bool tip   = lines && ((gl_VertexID & 1) == 1);
    uint id    = lines ? uint(gl_VertexID >> 1) : uint(gl_VertexID);
    uint s     = id * 2u + 1013904223u + (lines ? 2654435769u : 0u);

    float r1 = rnd(s), r2 = rnd(s), r3 = rnd(s), r4 = rnd(s), r5 = rnd(s);
    float r6 = rnd(s), r7 = rnd(s), r8 = rnd(s), r9 = rnd(s);

    // ---- base direction on the unit sphere (optionally snapped to lat/long grid lines) ----
    float z  = r2 * 2.0 - 1.0;
    float th = r1 * 6.2831853;
    if (r3 < uMesh)
    {
        const float N = 46.0;
        if (r4 < 0.5) z  = (floor((z * 0.5 + 0.5) * N) + 0.5) / N * 2.0 - 1.0;
        else          th = (floor(r1 * N * 1.5) + 0.5) / (N * 1.5) * 6.2831853;
    }
    float rxy = sqrt(max(1.0 - z * z, 0.0));
    vec3 d = vec3(rxy * cos(th), z, rxy * sin(th));

    // ---- displacement field ----
    vec3 q = d * uDetail + vec3(0.0, uFlow * 0.6, uFlow);
    vec3 w = vec3(vnoise(q * 1.3 + vec3(1.7, 9.2, 3.1)),
                  vnoise(q * 1.3 + vec3(8.3, 2.8, 5.5)),
                  vnoise(q * 1.3 + vec3(4.1, 6.6, 0.9)));
    float n   = fbm(q + w * 0.9);
    // beat explosion: every particle is thrown outward by a random amount, plus a ripple that travels over the surface
    float ring  = exp(-pow(((d.y * 0.5 + 0.5) - uWave) * 6.0, 2.0)) * uPunch;
    float burst = uPunch * (0.1 + 0.9 * r6) * 0.6;
    float rad = 1.0 + uWarp * n + ring * 0.55 + burst;
    vec3 pos  = d * rad;
    pos += (vec3(r6, r7, r8) - 0.5) * uFuzz;
    vec3 N3 = normalize(d + uWarp * 1.1 * w);

    if (lines)
    {
        float len = uFilLen * (0.08 + r9 * r9 * r9 * 1.5) * (1.0 + uPunch * 0.7);
        vec3 dirOut = normalize(d + 0.6 * w + vec3(0.0, uWind, 0.0));
        if (tip) pos += dirOut * len;
    }

    vec3 P  = rotAll(pos);
    vec3 Nr = rotAll(N3);

    const float camZ = 3.4;
    float persp = camZ / max(camZ - P.z, 1.4);   // clamp: stuff near the camera must not blow up
    vec2 ndc = P.xy * persp * 0.42 * uZoom;
    ndc.x *= uRes.y / uRes.x;
    gl_Position = vec4(ndc, 0.0, 1.0);
    gl_PointSize = clamp(uSize * (uRes.y / 1080.0) * persp * (0.7 + 0.6 * r5), 1.0, 24.0);

    // ---- shading: tint + diffuse + procedural studio reflection ----
    float t = clamp(0.5 + n * 0.9 + uColorShift + (r5 - 0.5) * 0.15, 0.0, 1.0);
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
    float a = uPerParticle * facing * fade;

    float rim = 0.55 + 1.5 * pow(max(1.0 - abs(ndv), 0.0), 2.0);
    vec3 inkCol = vec3(a * rim);
    vColor = vec4(mix(col * a, inkCol, uInk), 1.0);
}
