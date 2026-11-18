import csv, json, os, pathlib, re, subprocess
import numpy as np
from scipy.signal import butter,sosfilt
from scipy.optimize import differential_evolution
P=pathlib.Path('src/Engine.hpp'); OUT=pathlib.Path('spectral-results'); OUT.mkdir(exist_ok=True)
raw=P.read_text()
def rep(s,a,b):
 if a not in s: raise RuntimeError('missing source anchor: '+a)
 return s.replace(a,b,1)
# Reproduce the exact first-stage artifact coefficients before comparing.
base=rep(raw,'blend = ambienceMode?.180f:0.f;', 'blend = ambienceMode?0.166304902f:0.f;\n        if(ambienceMode){size*=0.634196061f;if(seconds<9.89f)seconds*=0.933686069f;}')
base=rep(base,'earlyGain = spec.early;', 'earlyGain = spec.early*(ambienceMode?1.78245406f:1.f);')
base=rep(base,'const float duration=std::max(5.f,(sp.shape==pan?60.f:85.f)*settings.decay[0]*sp.size);','const float duration=settings.program==nonlin2 ? std::max(5.f,110.183463f+54.5782088f*std::pow(settings.decay[0],0.665382957f)) : std::max(5.f,(sp.shape==pan?60.f:85.f)*settings.decay[0]*sp.size);')
base=rep(base,'const float amp=.105f*envelope;', 'const float amp=.105f*envelope*(settings.program==nonlin2?std::pow(std::max(.001f,t),1.41893564f)*std::pow(std::max(.001f,1-t),2.37573676f):1.f);')
rows=list(csv.DictReader(open('tests/nevo_spectral_targets.csv')))
rows=[r for r in rows if not r['setting'].startswith('Mono')]
bands=[125,250,500,1000,2000,4000,8000,16000]
filters={f:butter(2,[f/2**.5,f*2**.5],fs=48000,btype='bandpass',output='sos') for f in bands}
def build(s):
 P.write_text(s);subprocess.run(['g++','-O3','-std=c++20','tests/nevo_render_long.cpp','-o','/tmp/nevo2'],check=True)
def metrics(mode,setting):
 y=np.frombuffer(subprocess.check_output(['/tmp/nevo2',mode,setting]),dtype=np.float32).reshape(-1,2).astype(float)
 en=np.sum(y*y,axis=1); peak=en.max()
 if peak<=0:return {}
 start=np.argmax(en>peak*1e-6); y=y[start:]; en=en[start:];total=en.sum()
 def quant(e,q):return np.searchsorted(np.cumsum(e),e.sum()*q)/48
 def corr(z):return np.sum(z[:,0]*z[:,1])/max(1e-30,np.sqrt(np.sum(z[:,0]**2)*np.sum(z[:,1]**2)))
 d={f't{q}_ms':quant(en,q/1000) for q in [100,500,950,999]}; d['corr']=corr(y)
 for f in bands:
  z=sosfilt(filters[f],y,axis=0);e=np.sum(z*z,axis=1);et=e.sum()
  d[f'b{f}_db']=10*np.log10(max(et/total,1e-12))
  d[f'b{f}_corr']=corr(z);d[f'b{f}_t95']=quant(e,.95)
  if mode=='ambience':
   ed=10*np.log10(np.maximum((et-np.r_[0,np.cumsum(e)[:-1]])/max(et,1e-30),1e-12))
   idx=np.flatnonzero((ed<=-5)&(ed>=-25))
   if len(idx)>100:d[f'b{f}_rt']=-60/np.polyfit(idx/48000,ed[idx],1)[0]
 for a,b in [(0,20),(20,50),(50,100),(100,200),(200,400)]:
  z=y[a*48:b*48];blocks=z[:len(z)//240*240].reshape(-1,240,2)
  rms=np.sqrt(np.mean(blocks**2,axis=1,keepdims=True));valid=rms[:,0,:]>np.sqrt(peak)*1e-4
  if valid.any():d[f'd{a}']=np.mean((np.mean(np.abs(blocks)>rms,axis=1)/.3173105)[valid])
  d[f'e{a}']=np.sum(z*z)/total
 return d
def family(k):
 if k.endswith('_db'):return 'spectrum'
 if k.endswith('_rt'):return 'band_decay'
 if k.endswith('_corr') or k=='corr':return 'stereo'
 if k.startswith('d'):return 'density'
 if k.startswith('e'):return 'envelope'
 return 'timing'
def tolerance(k,t):
 f=family(k)
 if f=='spectrum':return 1.
 if f=='stereo':return .05
 if f=='density':return max(.05,abs(t)*.1)
 if f=='envelope':return max(.01,abs(t)*.1)
 return max(.001,abs(t)*.1)
def pairs(r,m):
 for k,v in r.items():
  if k in ('mode','setting') or not v:continue
  t=float(v); y=m.get(k,float('nan')); yield k,t,y,(y-t)/tolerance(k,t)
def score(r,m):
 groups={}
 for k,t,y,e in pairs(r,m):groups.setdefault(family(k),[]).append(min(400,e*e) if np.isfinite(e) else 400)
 return np.mean([np.mean(v) for v in groups.values()])
def report(name):
 with open(OUT/(name+'.csv'),'w') as f:
  w=csv.writer(f);w.writerow(['mode','setting','split','metric','family','target','model','error_in_tolerance_units','pass'])
  for mode in ['ambience','nonlin']:
   for i,r in enumerate(x for x in rows if x['mode']==mode):
    m=metrics(mode,r['setting'])
    for k,t,y,e in pairs(r,m):w.writerow([mode,r['setting'],'fit' if i%2==0 else 'holdout',k,family(k),t,y,e,abs(e)<=1])
build(base);report('first_stage')
s=rep(base,'#include <limits>','#include <limits>\n#include <cstdlib>\ninline float nevo2(const char* n,float d){const char* v=std::getenv(n);return v?std::strtof(v,nullptr):d;}')
s=rep(s,'blend = ambienceMode?0.166304902f:0.f;','blend = ambienceMode?nevo2("A_BLEND",0.166304902f):0.f;')
s=rep(s,'size*=0.634196061f;if(seconds<9.89f)seconds*=0.933686069f;','size*=nevo2("A_SIZE",0.634196061f);if(seconds<9.89f)seconds*=nevo2("A_RT",0.933686069f);')
s=rep(s,'earlyGain = spec.early*(ambienceMode?1.78245406f:1.f);','earlyGain = spec.early*(ambienceMode?nevo2("A_EARLY",1.78245406f):1.f);\n        diffScale=ambienceMode?nevo2("A_DIFF",1.f):1.f;')
s=rep(s,'std::min(highCorner,4200.f)','std::min(highCorner,nevo2("A_HZ",4200.f))')
s=rep(s,'(infinite ? 9.9f : seconds)*highScale','(infinite ? 9.9f : seconds)*highScale*(ambienceMode?nevo2("A_HRT",1.f):1.f)')
s=rep(s,'spread[i]*size','spread[i]*size*diffScale')
s=rep(s,'float size=1, earlyGain','float diffScale=1;\n    float size=1, earlyGain')
s=rep(s,'110.183463f+54.5782088f*std::pow(settings.decay[0],0.665382957f)','nevo2("N_BASE",110.183463f)+nevo2("N_SCALE",54.5782088f)*std::pow(settings.decay[0],nevo2("N_POWER",0.665382957f))')
s=rep(s,'1.41893564f)*std::pow(std::max(.001f,1-t),2.37573676f)','nevo2("N_RISE",1.41893564f))*std::pow(std::max(.001f,1-t),nevo2("N_FALL",2.37573676f))')
# Independent wet-path coloration; no reference waveform or convolution in audio path.
s=rep(s,'inGain=dbGain(settings.input);','calMode=p==ambience || p==nonlin2;\n        calPole=coefficient(p==ambience?nevo2("A_HP",20.f):nevo2("N_HP",20.f));\n        calMix=p==ambience?nevo2("A_LCUT",0.f):nevo2("N_LCUT",0.f);\n        inGain=dbGain(settings.input);')
s=rep(s,'time=0; inputPeak=0;','time=0; inputPeak=0;calState.fill(0);')
s=rep(s,'++time;\n        for(auto& channel:out)','if(calMode) for(size_t ch=0;ch<2;++ch){calState[ch]+=calPole*(out[ch]-calState[ch]);out[ch]-=calMix*calState[ch];}\n        ++time;\n        for(auto& channel:out)')
s=rep(s,'float inGain=1,outGain=1,inputPeak=0;','std::array<float,2> calState{}; float calPole=0,calMix=0; bool calMode=false;\n    float inGain=1,outGain=1,inputPeak=0;')
build(s)
specs=[('ambience',['A_BLEND','A_SIZE','A_RT','A_EARLY','A_DIFF','A_HZ','A_HRT','A_HP','A_LCUT'],[(.05,.3),(.5,1.2),(.65,1.25),(.1,3),(0.5,1.8),(1500,12000),(.2,1.5),(200,3500),(.2,1)]),('nonlin',['N_BASE','N_SCALE','N_POWER','N_RISE','N_FALL','N_HP','N_LCUT'],[(60,160),(25,90),(.4,.95),(.2,2.5),(.8,3.5),(200,3500),(.2,1)])]
best={}
for mode,names,bounds in specs:
 subset=[r for r in rows if r['mode']==mode][::2]
 def cost(x):
  os.environ.update({n:str(v) for n,v in zip(names,x)})
  return np.mean([score(r,metrics(mode,r['setting'])) for r in subset])
 res=differential_evolution(cost,bounds,maxiter=10,popsize=4,seed=525,polish=False,tol=.015)
 best.update(dict(zip(names,map(float,res.x))))
 os.environ.update({n:str(v) for n,v in best.items()})
 (OUT/'parameters.json').write_text(json.dumps(best,indent=2))
 print(mode,res.fun,best,flush=True)
# Bake constants in production code, not environment variables.
s=re.sub(r'nevo2\("([A-Z_]+)",[^)]+\)',lambda m: f'{best[m[1]]:.9f}f',s)
s=s.replace('#include <cstdlib>\ninline float nevo2(const char* n,float d){const char* v=std::getenv(n);return v?std::strtof(v,nullptr):d;}','')
assert 'nevo2(' not in s
build(s);report('candidate')
(OUT/'Engine.hpp').write_text(s)
# Guardrail: publish separate metrics, don't claim every metric passes.
for file in ['first_stage','candidate']:
 rr=list(csv.DictReader(open(OUT/(file+'.csv'))))
 for mode in ['ambience','nonlin']:
  for split in ['fit','holdout']:
   for fam in sorted(set(r['family'] for r in rr)):
    z=[r for r in rr if r['mode']==mode and r['split']==split and r['family']==fam]
    if z:print(file,mode,split,fam,sum(r['pass']=='True' for r in z),'/',len(z),flush=True)
