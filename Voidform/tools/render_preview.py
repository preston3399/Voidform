#!/usr/bin/env python3
"""Headless preview of the Voidform shaders (mirrors Renderer.cpp). Usage: render_preview.py out.png [preset_idx ...]"""
import sys, json, math, colorsys, os
import numpy as np, moderngl
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SH = os.path.join(ROOT, 'Shaders')
def rd(n): return open(os.path.join(SH, n)).read()

DEFAULT_AUDIO = dict(sens=1.0)
TARGETS = ['size','warp','spin','filaments','bloom','glitch','brightness','color']

def hsv(h, s, v): return colorsys.hsv_to_rgb(h % 1.0, s, v)

def effective(p, m):
    pu = m.get('punch', 0.0)
    e = {}
    e['zoom']    = p['zoom'] * (1 + 0.50 * m['size'] + 0.12 * pu)
    e['warp']    = p['warp'] + 1.1 * m['warp'] + 0.25 * pu
    e['spinAdd'] = m['spin'] * 4.0 + 1.5 * pu
    e['filaments'] = min(1.0, p['filaments'] + 0.5 * m['filaments'] + 0.1 * pu)
    e['filLen']  = p['filLen'] * (1 + 1.2 * m['filaments'])
    e['bloom']   = p['bloom'] * (1 + 1.5 * m['bloom'] + 0.5 * pu) + 0.9 * m['bloom']
    e['glitch']  = min(1.5, p['glitch'] + m['glitch'] + 0.35 * pu)
    e['bright']  = p['brightness'] * (1 + 0.7 * m['brightness'] + 0.25 * pu)
    e['cshift']  = 0.5 * m['color']
    e['chroma']  = p['chroma'] + 0.6 * pu
    return e

class Viz:
    def __init__(self, W=660, H=660):
        self.W, self.H = W, H
        self.ctx = c = moderngl.create_standalone_context(backend='egl', require=330)
        fs = rd('fullscreen.vert')
        mk = lambda f: c.program(vertex_shader=fs, fragment_shader=rd(f))
        self.pFade, self.pDown, self.pUp, self.pComp = mk('fade.frag'), mk('blur_down.frag'), mk('blur_up.frag'), mk('composite.frag')
        self.pPart = c.program(vertex_shader=rd('particles.vert'), fragment_shader=rd('particles.frag'))
        self.vao = lambda pr: c.vertex_array(pr, [])
        self.tex = [c.texture((W, H), 4, dtype='f2') for _ in range(2)]
        for t in self.tex: t.repeat_x = t.repeat_y = False
        self.fbo = [c.framebuffer([t]) for t in self.tex]
        self.cur = 0
        self.mips = []
        w, h = W, H
        for i in range(5):
            w, h = max(1, w // 2), max(1, h // 2)
            t = c.texture((w, h), 4, dtype='f2'); t.repeat_x = t.repeat_y = False
            self.mips.append((t, c.framebuffer([t]), w, h))
        self.out = c.texture((W, H), 4); self.outf = c.framebuffer([self.out])
        self.t = 0.0; self.fitS = 1.0; self.flow = 0.0; self.rotY = 0.0
        c.enable(moderngl.PROGRAM_POINT_SIZE)

    @staticmethod
    def U(pr, name, v):
        if name in pr:
            pr[name].value = v

    def step(self, p, m, dt=1/60.):
        c, W, H = self.ctx, self.W, self.H
        e = effective(p, m); pu0 = m.get('punch', 0.0)
        self.t += dt
        self.flow += dt * p['speed'] * (1 + 0.8 * m['warp'])
        self.rotY += dt * (p['spin'] + e['spinAdd'])
        pu = m.get('punch', 0.0); shake = p.get('shake', 0.5) * pu
        ink = 1.0 if p['ink'] else 0.0
        ext = 1 + max(0, e['warp']) * 0.75 + pu0 * 1.5 + p['fuzz'] * 0.6 + (e['filLen'] * (0.8 + 0.8 * pu0) * min(1, e['filaments'] * 3) if e['filaments'] > 0.01 else 0)
        target = min(1.0, max(0.25, 1.55 / ext)) * min(1.0, self.W / self.H)
        target = 1.0 + p.get('fit', 1.0) * (target - 1.0)
        if target < self.fitS: self.fitS = target
        else: self.fitS += (target - self.fitS) * (1 - math.exp(-dt / 0.25))
        e['zoom'] *= self.fitS

        # fade / trails
        prev, nxt = self.tex[self.cur], self.cur ^ 1
        self.fbo[nxt].use(); c.viewport = (0, 0, W, H); c.disable(moderngl.BLEND)
        if p['trails'] > 0.001:
            prev.use(0); self.U(self.pFade, 'uPrev', 0); self.U(self.pFade, 'uDecay', p['trails'])
            self.vao(self.pFade).render(moderngl.TRIANGLES, vertices=3)
        else:
            c.clear(0, 0, 0, 0)
        self.cur = nxt

        # particles
        count = int(p['count'])
        O = count * (math.pi / 4 * p['size'] ** 2 * 0.8) * (H / 1080.) ** 2 / (math.pi * (0.21 * e['zoom'] * H) ** 2)
        per = e['bright'] * 0.55 * max(O, 0.05) ** -0.55 * (1.0 if not ink else 0.9)
        pr = self.pPart
        a = hsv(p['hueA'], p['satA'], 1.0); b = hsv(p['hueB'], p['satB'], 1.0)
        for k, v in dict(uRes=(W, H), uFlow=self.flow, uRotY=self.rotY + shake * 0.10 * math.sin(self.t * 41 + 1.3), uRotX=p['tilt'] + 0.12 * math.sin(self.t * 0.23) + shake * 0.14 * math.sin(self.t * 53), uPunch=pu, uWave=m.get('wave', 2.0),
                         uZoom=e['zoom'], uSize=p['size'], uWarp=e['warp'], uDetail=p['detail'], uMesh=p['mesh'], uFuzz=p['fuzz'],
                         uFilLen=e['filLen'], uWind=p['wind'], uMetal=p['metallic'], uReflect=p['reflect'], uPerParticle=per,
                         uColorMix=p['colorMix'], uColorShift=e['cshift'], uInk=ink, uColA=a, uColB=b).items():
            self.U(pr, k, v)
        c.enable(moderngl.BLEND); c.blend_func = moderngl.ONE, moderngl.ONE
        self.U(pr, 'uMode', 0); self.vao(pr).render(moderngl.POINTS, vertices=count)
        nfil = int(80000 * e['filaments'] * min(1.0, count / 300000 + 0.3))
        if nfil > 0:
            self.U(pr, 'uMode', 1); self.U(pr, 'uPerParticle', per * 0.8)
            self.vao(pr).render(moderngl.LINES, vertices=nfil * 2)

        # bloom chain
        c.disable(moderngl.BLEND)
        src = self.tex[self.cur]
        for i, (t, f, w, h) in enumerate(self.mips):
            f.use(); c.viewport = (0, 0, w, h)
            src.use(0); self.U(self.pDown, 'uTex', 0); self.U(self.pDown, 'uHalfPixel', (0.5 / w, 0.5 / h))
            self.vao(self.pDown).render(moderngl.TRIANGLES, vertices=3); src = t
        c.enable(moderngl.BLEND); c.blend_func = moderngl.ONE, moderngl.ONE
        for i in range(len(self.mips) - 1, 0, -1):
            t, f, w, h = self.mips[i]; t2, f2, w2, h2 = self.mips[i - 1]
            f2.use(); c.viewport = (0, 0, w2, h2); t.use(0)
            self.U(self.pUp, 'uTex', 0); self.U(self.pUp, 'uHalfPixel', (0.5 / w, 0.5 / h)); self.U(self.pUp, 'uWeight', 0.85)
            self.vao(self.pUp).render(moderngl.TRIANGLES, vertices=3)
        c.disable(moderngl.BLEND)

        # composite
        self.outf.use(); c.viewport = (0, 0, W, H)
        self.tex[self.cur].use(0); self.mips[0][0].use(1)
        pc = self.pComp
        absorb = tuple(1 - p['satA'] * (1 - x) for x in a) if False else tuple((1 - p['satA']) + p['satA'] * (1 - x) for x in hsv(p['hueA'], 1.0, 1.0))
        for k, v in dict(uScene=0, uBloom=1, uRes=(W, H), uTime=self.t, uBloomAmt=e['bloom'] * 0.35, uGrain=p['grain'], uChroma=e['chroma'],
                         uGlitch=e['glitch'], uVignette=p['vignette'], uExposure=1.0, uInk=ink, uCrt=float(p.get('frame',0)), uBg=p['bg'], uAbsorb=absorb).items():
            self.U(pc, k, v)
        self.vao(pc).render(moderngl.TRIANGLES, vertices=3)
        return Image.frombytes('RGBA', (W, H), self.out.read()).transpose(Image.FLIP_TOP_BOTTOM).convert('RGB')

def zero(): return {k: 0.0 for k in TARGETS}

if __name__ == '__main__':
    out = sys.argv[1]
    presets = json.load(open(os.path.join(ROOT, 'Presets', 'presets.json')))
    idx = [int(x) for x in sys.argv[2:]] or range(len(presets))
    sheet = []
    for i in idx:
        v = Viz(); p = dict(presets[i]['values'])
        frames = []
        rng = np.random.default_rng(i)
        for f in range(90):
            ph = f / 60.
            kick = max(0, 1 - ((ph * 2.0) % 1.0) * 3.0)
            m = zero(); m['size'] = kick * 0.9; m['warp'] = 0.5 + 0.3 * math.sin(ph * 3); m['spin'] = 0.1
            m['filaments'] = 0.3 + 0.4 * abs(math.sin(ph * 5)); m['bloom'] = kick * 0.6; m['brightness'] = kick * 0.4
            m['glitch'] = kick * 0.3
            img = v.step(p, m)
            if f in (30, 60, 89): frames.append(img)
        sheet.append(frames)
    W = sheet[0][0].width
    S = Image.new('RGB', (W * 3, W * len(sheet)))
    for r, fr in enumerate(sheet):
        for c_, im in enumerate(fr): S.paste(im, (c_ * W, r * W))
    S.resize((S.width // 2, S.height // 2)).save(out)
    print('ok', out)
