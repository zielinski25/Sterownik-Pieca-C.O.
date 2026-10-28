from pathlib import Path
import subprocess,tempfile,hashlib,json,os
P=Path(__file__).resolve().parent;N=P.parent;U=N/'blokada-startu-fs196/upstream'
source=json.loads((U/'SOURCES.json').read_text())
for name in ['FS.cpp','FS.h','FSImpl.h','LittleFS.h']:
 ref=next(v for k,v in source.items() if k.endswith('/'+name));assert hashlib.sha256((U/name).read_bytes()).hexdigest()==ref['sha256']
base=P/'WYNIKI';base.mkdir(exist_ok=True);i=1
while (base/f'run{i:03d}').exists():i+=1
out=base/f'run{i:03d}';out.mkdir()
inputs=[*P.glob('*.h'),*P.glob('*.cpp'),*P.glob('*.py'),*P.glob('*.inc'),*P.glob('upstream/*'),*P.glob('host/*'),U/'SOURCES.json',U/'FS.cpp',U/'FS.h',U/'FSImpl.h',U/'LittleFS.h']
r={'status':'RUNNING','release_ready':False,'firmware_modified':False,'inputs':{str(p.relative_to(N)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs if p.is_file()},'profiles':{}}
def save():(out/'wynik.json').write_text(json.dumps(r,indent=2)+'\n')
try:
 with tempfile.TemporaryDirectory() as td:
  d=Path(td)
  for profile,flags in [('native',[]),('asan-ubsan',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'])]:
   cmd=['c++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-exceptions','-fno-rtti',*flags,*['-I'+str(x) for x in [P,P/'host',U]],P/'Test.cpp',P/'Allocations.cpp',U/'FS.cpp','-o',d/'test']
   t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,t.stderr
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1');t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=30,env=env);r['profiles'][profile]={'stdout':t.stdout,'stderr':t.stderr,'returncode':t.returncode};save();assert t.returncode==0,(t.stdout,t.stderr);print(t.stdout)
 r['status']='PASS';save();print(out)
except Exception as e:r['status']='FAIL';r['error']=str(e);save();raise
