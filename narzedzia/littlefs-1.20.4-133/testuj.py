from pathlib import Path
import hashlib,json,subprocess,tempfile,zipfile
P=Path(__file__).resolve().parent;N=P.parent
refs=json.loads((P/'ZRODLA.json').read_text())
for n,v in refs['files'].items():
 b=(P/'zrodla'/n).read_bytes();assert len(b)==v['bytes'] and hashlib.sha256(b).hexdigest()==v['sha256']
base=P/'WYNIKI';base.mkdir(exist_ok=True);i=1
while (base/f'run{i:03d}').exists():i+=1
out=base/f'run{i:03d}';out.mkdir()
inputs=[N/'kwalifikacja-zapisu-esp128/test_error_latch.c',N/'straznik-zapisu129/WriteSession.h',N/'straznik-zapisu129/WriteSession.cpp',N/'straznik-zapisu129/test_session.cpp',N/'straznik-zapisu129/test_littlefs.cpp',P/'testuj.py',*sorted((P/'zrodla').iterdir())]
result={'status':'RUNNING','scope':'upstream of esp_littlefs1.20.4, RAM callbacks, not ESP VFS or flash','release_ready':False,'inputs':{str(p.relative_to(N)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'commands':[]}
def save():(out/'wynik.json').write_text(json.dumps(result,indent=2)+'\n')
with zipfile.ZipFile(out/'sources.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in inputs:z.write(p,p.relative_to(N))
def run(cmd,label,cwd):
 r=subprocess.run(list(map(str,cmd)),cwd=cwd,capture_output=True,text=True,timeout=60)
 (out/(label+'.stdout')).write_text(r.stdout);(out/(label+'.stderr')).write_text(r.stderr)
 result['commands'].append({'label':label,'exit':r.returncode,'command':list(map(str,cmd))});save();assert r.returncode==0,r.stdout+r.stderr
 return r
try:
 with tempfile.TemporaryDirectory(prefix='littlefs133-') as tmp:
  t=Path(tmp)
  for profile,extra in [('native',[]),('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-g'])]:
   flags=['-Wall','-Wextra','-Werror',*extra,'-I'+str(P/'zrodla'),'-I'+str(N/'straznik-zapisu129')]
   objs=[]
   for n in ['lfs','lfs_util']:
    obj=t/(n+'.o');run(['cc','-std=c99',*flags,'-c',P/'zrodla'/(n+'.c'),'-o',obj],profile+'-compile-'+n,t);objs.append(obj)
   cases=[('core',['cc','-std=c99',*flags,N/'kwalifikacja-zapisu-esp128/test_error_latch.c',*objs]),('guard',['c++','-std=c++11',*flags,N/'straznik-zapisu129/WriteSession.cpp',N/'straznik-zapisu129/test_littlefs.cpp',*objs]),('unit',['c++','-std=c++11',*flags,N/'straznik-zapisu129/WriteSession.cpp',N/'straznik-zapisu129/test_session.cpp'])]
   for label,cmd in cases:
    run([*cmd,'-o',t/'test'],profile+'-'+label+'-compile',t)
    r=run([t/'test'],profile+'-'+label+'-run',t);assert not r.stderr and 'PASS' in r.stdout
 result['status']='PASS';save();print('PASS:3 core scenarios +3 guard integrations +23 unit groups in each of native/sanitized;',out)
except Exception as e:
 result['status']='FAIL';result['error']=str(e);save();raise
