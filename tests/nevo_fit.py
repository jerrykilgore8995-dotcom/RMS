import csv, pathlib, subprocess, numpy as np
from scipy.optimize import differential_evolution
root=pathlib.Path('calibration-results');root.mkdir(exist_ok=True)
engine=pathlib.Path('src/Engine.hpp'); original=engine.read_text()
rows=list(csv.DictReader(open('tests/nevo_targets.csv')))
keys=['t10_ms','t50_ms','t95_ms','t999_ms','correlation']
def compile_engine():
 subprocess.run(['g++','-O3','-std=c++20','tests/nevo_render.cpp','-o','/tmp/nevo_render'],check=True)
def measure(mode,setting):
 y=np.frombuffer(subprocess.check_output(['/tmp/nevo_render',mode,str(setting)]),dtype=np.float32).reshape(-1,2).astype(float)
 en=np.sum(y*y,axis=1); cs=np.cumsum(en)
 if cs[-1]<=0: raise ValueError('silent render')
 return np.r_[np.searchsorted(cs,cs[-1]*np.array([.1,.5,.95,.999]))/48,np.dot(y[:,0],y[:,1])/np.sqrt(np.sum(y[:,0]**2)*np.sum(y[:,1]**2))]
def error(y,t):
 return np.r_[(y[:4]-t[:4])/np.maximum(t[:4],1),(y[4]-t[4])/max(abs(t[4]),.05)]
def report(name):
 compile_engine()
 with open(root/(name+'.csv'),'w') as f:
  w=csv.writer(f);w.writerow(['mode','setting']+[q+'_'+k for k in keys for q in ['target','model','error']])
  for r in rows:
   t=np.array([float(r[k]) for k in keys]); y=measure(r['mode'],r['setting']); z=error(y,t)
   w.writerow([r['mode'],r['setting']]+[v for a,b,c in zip(t,y,z) for v in (a,b,c)])
report('baseline')
# Optimize each mode separately; hold out alternating settings.
# Compile once with runtime knobs passed through environment, then freeze best values.
src=original.replace('#include <limits>','#include <limits>\n#include <cstdlib>\ninline float nevoKnob(const char* n,float d){const char* v=std::getenv(n);return v?std::strtof(v,nullptr):d;}')
src=src.replace('blend = ambienceMode?.180f:0.f;', 'blend = ambienceMode?nevoKnob("A_BLEND",.180f):0.f;\n        if(ambienceMode){size*=nevoKnob("A_SIZE",1.f);if(seconds<9.89f)seconds*=nevoKnob("A_DECAY",1.f);}')
src=src.replace('earlyGain = spec.early;', 'earlyGain = spec.early*(ambienceMode?nevoKnob("A_EARLY",1.f):1.f);')
src=src.replace('const float duration=std::max(5.f,(sp.shape==pan?60.f:85.f)*settings.decay[0]*sp.size);','const float duration=settings.program==nonlin2 ? std::max(5.f,nevoKnob("N_BASE",100.f)+nevoKnob("N_SCALE",30.f)*std::pow(settings.decay[0],nevoKnob("N_POWER",1.f))) : std::max(5.f,(sp.shape==pan?60.f:85.f)*settings.decay[0]*sp.size);')
src=src.replace('const float amp=.105f*envelope;', 'const float amp=.105f*envelope*(settings.program==nonlin2?std::pow(std::max(.001f,t),nevoKnob("N_RISE",0.f))*std::pow(std::max(.001f,1-t),nevoKnob("N_FALL",0.f)):1.f);')
assert src!=original
engine.write_text(src);compile_engine()
import os,json
best={}
for mode,names,bounds in [('ambience',['A_BLEND','A_SIZE','A_DECAY','A_EARLY'],[(.05,.3),(.6,1.8),(.4,1.5),(.1,4)]),('nonlin',['N_BASE','N_SCALE','N_POWER','N_RISE','N_FALL'],[(40,140),(10,100),(.3,1.2),(0,3),(0,3)])]:
 subset=[r for r in rows if r['mode']==mode][::2]
 def cost(x):
  os.environ.update({n:str(v) for n,v in zip(names,x)})
  return np.mean([np.mean(error(measure(mode,r['setting']),np.array([float(r[k]) for k in keys]))**2) for r in subset])
 res=differential_evolution(cost,bounds,maxiter=12,popsize=5,seed=525,polish=False)
 best.update(dict(zip(names,map(float,res.x))))
 os.environ.update({n:str(v) for n,v in best.items()})
 print(mode,res.fun,best,flush=True)
(root/'parameters.json').write_text(json.dumps(best,indent=2))
import re
src=re.sub(r'nevoKnob\("([A-Z_]+)",[^)]+\)',lambda m: format(best[m[1]],'.9g')+'f' if '.' in format(best[m[1]],'.9g') else format(best[m[1]],'.9g')+'.0f',src)
src=src.replace('#include <cstdlib>\ninline float nevoKnob(const char* n,float d){const char* v=std::getenv(n);return v?std::strtof(v,nullptr):d;}','')
engine.write_text(src)
report('candidate')
(root/'Engine.hpp').write_text(src)
