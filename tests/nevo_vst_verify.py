import ast,csv,pathlib,sys,numpy as np
from scipy.signal import butter,sosfilt
from pedalboard import load_plugin
# Reuse identical metrics without running the fitter or modifying source.
source=pathlib.Path('tests/nevo_spectral_fit.py').read_text()
tree=ast.parse(source)
node=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='metrics')
fn=ast.get_source_segment(source,node)
fn=fn.replace('def metrics(mode,setting):','def metrics(mode,y):')
fn='\n'.join(line for line in fn.splitlines() if 'subprocess.check_output' not in line)
bands=[125,250,500,1000,2000,4000,8000,16000]
filters={f:butter(2,[f/2**.5,f*2**.5],fs=48000,btype='bandpass',output='sos') for f in bands}
exec(fn)
expected=list(csv.DictReader(open('spectral-results/candidate.csv')))
report=[];failed=False
for mode,setting in dict.fromkeys((r['mode'],r['setting']) for r in expected):
 p=load_plugin(sys.argv[1])
 print('parameters',list(p.parameters),flush=True)
 p.algorithm=1 if mode=='ambience' else 8
 p.decay=float(setting)
 x=np.zeros((2,48000*(10 if mode=='ambience' else 1)),dtype=np.float32);x[:,0]=.8
 y=p(x,48000,buffer_size=512,reset=True).T.astype(float)
 if not np.isfinite(y).all():raise ValueError('nonfinite VST3 output')
 m=metrics(mode,y)
 for r in expected:
  if (r['mode'],r['setting'])!=(mode,setting):continue
  k=r['metric']; t=float(r['model']);v=m.get(k,float('nan'))
  tol=.15 if k.endswith('_db') else .02 if k.endswith('_corr') or k=='corr' or k.startswith(('d','e')) else max(.002,abs(t)*.03)
  ok=np.isfinite(v) and np.isfinite(t) and abs(v-t)<=tol
  report.append([mode,setting,k,t,v,tol,ok]);failed|=not ok
with open('spectral-results/vst3_verification.csv','w',newline='') as f:
 w=csv.writer(f); w.writerow(['mode','setting','metric','engine','vst3','tolerance','pass']);w.writerows(report)
print('VST3 parity',sum(r[-1] for r in report),'/',len(report),flush=True)
if failed:sys.exit(1)
