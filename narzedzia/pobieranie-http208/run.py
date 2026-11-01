from pathlib import Path
import tempfile,subprocess,hashlib,json,os
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent
H=N/'uchwyty-runtime197';M=N/'most-runtime198';U=N/'blokada-startu-fs196/upstream'
A=N/'odpowiedz-http199';D=N/'odpowiedz-odroczona203';S=N/'pobieranie-historia207';C=N/'naglowki-upload206/host'
subprocess.run(['python3','-B',str(P/'build.py')],check=True)
base=P/'WYNIKI';base.mkdir(exist_ok=True);i=1
while (base/f'run{i:03d}').exists():i+=1
out=base/f'run{i:03d}';out.mkdir()
inputs=[*P.glob('*.h'),*P.glob('*.inc'),*P.glob('*.py'),*P.glob('*.cpp'),H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp',U/'FS.h',U/'FSImpl.h',U/'LittleFS.h',A/'Abstract.inc',A/'Transport.inc',A/'CheckedFileBody.h',D/'Methods.inc',D/'Fields.inc',D/'API.inc',S/'Snapshots.h',C/'Arduino.h']
r={'status':'RUNNING','release_ready':False,'main_changed':False,'inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'profiles':{}}
def save():(out/'wynik.json').write_text(json.dumps(r,indent=2)+'\n')
try:
 with tempfile.TemporaryDirectory() as td:
  for label,flags in [('native',[]),('asan-ubsan',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'])]:
   for flow in [0,1]:
    exe=Path(td)/'test';cmd=['c++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-exceptions','-fno-rtti',*flags,'-DASYNCWEBSERVER_USE_CHUNK_INFLIGHT='+str(flow),
     *['-I'+str(x) for x in [C,P,S,H,H/'upstream',M,U,A,D]],P/'Test.cpp',U/'FS.cpp','-o',exe]
    c=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=180);assert c.returncode==0,c.stderr
    t=subprocess.run([exe],capture_output=True,text=True,timeout=120,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
    r['profiles'][label+'-flow'+str(flow)]={'stdout':t.stdout,'stderr':t.stderr,'returncode':t.returncode};save();assert t.returncode==0,(t.stdout,t.stderr);print(t.stdout)
 r['status']='PASS';save();print(out)
except Exception as e:
 r['status']='FAIL';r['error']=str(e);save();raise
