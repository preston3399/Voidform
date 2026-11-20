import sys, json, math, subprocess, wave, numpy as np
from render_preview import *
wav=wave.open('/home/claude/test_audio.wav'); sr=wav.getframerate()
x=np.frombuffer(wav.readframes(wav.getnframes()),dtype=np.int16).astype(np.float32)/32768
N=4096; win=np.hanning(N).astype(np.float32)
def norm(a,f):
    db=20*np.log10(max(a,1e-6))+3*np.log2(max(f,20)/1000); return min(1,max(0,(db+60)/54))**1.6
def band(mag,f,w):
    h=2**(w/2); lo,hi=f/h,f*h; bh=sr/N
    b0=min(max(1,int(lo//bh)),N//2-1); b1=min(max(b0,int(math.ceil(hi/bh))),N//2-1)
    return norm(mag[b0:b1+1].max(),math.sqrt(lo*hi))
bands=[(60,1.2,1.0,'size'),(220,2.0,0.9,'warp'),(1800,2.5,0.6,'brightness'),(9000,2.0,0.8,'filaments')]
FPS=30; nf=int(len(x)/sr*FPS); dt=1/FPS
envs=np.zeros(4); allmods=[]; slow=0.0; punch=0.0; waveT=2.0; trig=False
REACT,PUNCH=1.6,1.5
for f in range(nf):
    end=int(f*sr/FPS)+N//2; seg=x[max(0,end-N):end]; seg=np.pad(seg,(N-len(seg),0))
    mag=np.abs(np.fft.rfft(seg*win))[:N//2]*4/N
    m=zero()
    for i,(fr,w,g,t) in enumerate(bands):
        lv=band(mag,fr,w)*g
        c=1-math.exp(-dt*1000/(8 if lv>envs[i] else 250)); envs[i]+=c*(lv-envs[i]); m[t]+=min(envs[i],2)
    for k in m: m[k]=min(m[k]*REACT,3.0)
    fast=max(envs[0],envs[1]*0.8); slow+=(1-math.exp(-dt/0.12))*(fast-slow)
    onset=min(1.5,max(0.0,(fast-slow)*5.0)); punch=min(1.3,max(punch*math.exp(-dt/0.22),onset*PUNCH))
    if onset>0.3 and not trig: trig=True; waveT=0.0
    if onset<0.1: trig=False
    waveT+=dt*2.2; m['punch']=punch; m['wave']=waveT
    allmods.append(m)
print('band peaks', {k:round(max(m[k] for m in allmods),2) for k in ('size','warp','brightness','filaments')})
presets=json.load(open('../Presets/presets.json'))
names=['ink','glow','metal']
for pi in range(3):
    p=dict(presets[pi]['values']); v=Viz(540,540); import os; d=f'/home/claude/af{pi}'; os.makedirs(d,exist_ok=True)
    for f,m in enumerate(allmods): v.step(p,m,dt).save(f'{d}/{f:04d}.png')
    subprocess.run(['ffmpeg','-v','error','-y','-framerate',str(FPS),'-i',f'{d}/%04d.png','-i','/mnt/user-data/uploads/c61a61ff-Chee_deep_bass_cut_.mp3','-map','0:v','-map','1:a','-c:v','libx264','-pix_fmt','yuv420p','-crf','20','-shortest',f'/home/claude/voidform_{names[pi]}.mp4'],check=True)
    print('done',names[pi])
