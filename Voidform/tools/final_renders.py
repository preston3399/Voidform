import json, math, sys
from render_preview import *
presets=json.load(open('../Presets/presets.json'))
def run(p, kick, frames=40, size=560):
    v=Viz(size,size); img=None
    for f in range(frames):
        ph=f/frames; k=kick*math.exp(-ph*2.0) if f>25 else kick*0.2
        m=zero(); m['size']=k*0.8; m['warp']=0.3+0.2*math.sin(f*.2); m['spin']=0.05
        m['filaments']=0.15+0.3*k; m['bloom']=0.4*k; m['brightness']=0.3*k; m['glitch']=0.4*k
        img=v.step(p,m)
    return img
S=Image.new('RGB',(560*3,560))
for i in range(3): S.paste(run(dict(presets[i]['values']),1.0),(560*i,0))
S.save('/home/claude/test_looks.png')
base=dict(presets[1]['values']); rows=[('count 60k',dict(count=60000)),('count 1M',dict(count=1000000)),('metallic+reflect',dict(metallic=1,reflect=1.6,bloom=0.2,brightness=0.5,filaments=0.1,colorMix=0.0,satA=0.0,satB=0.0)),('grain max / no bloom',dict(grain=1.5,bloom=0.0)),('hue A red, B blue',dict(hueA=0.0,satA=0.9,hueB=0.6,satB=0.9,colorMix=1.0,brightness=0.5,filaments=0.2))]
S=Image.new('RGB',(448*5,448))
for j,(n,o) in enumerate(rows):
    p=dict(base); p.update(o); S.paste(run(p,0.4,frames=14,size=448),(448*j,0))
S.save('/home/claude/test_sliders.png')
print('ok')
