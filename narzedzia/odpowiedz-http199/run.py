from pathlib import Path
import tempfile,subprocess,hashlib,json,os
P=Path(__file__).resolve().parent;R=P.parents[1];H=P.parent/'uchwyty-runtime197';M=P.parent/'most-runtime198';U=P.parent/'blokada-startu-fs196/upstream'
subprocess.run(['python3','-B',str(P/'build.py')],check=True)
base=P/'WYNIKI';base.mkdir(exist_ok=True);i=1
while (base/f'run{i:03d}').exists():i+=1
out=base/f'run{i:03d}';out.mkdir()
inputs=[p for p in P.rglob('*') if p.is_file() and ('WYNIKI' not in p.parts) and p.suffix in ['.h','.cpp','.inc','.py']]+[H/'RuntimeFilesystem.h',H/'CheckedRead.h',M/'Model197.inc',U/'FS.cpp',U/'FS.h',U/'FSImpl.h',H/'upstream/WebResponses.cpp']
r={'status':'RUNNING','release_ready':False,'inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'profiles':{}}
def save():(out/'wynik.json').write_text(json.dumps(r,indent=2)+'\n')
try:
 with tempfile.TemporaryDirectory() as td:
  for profile,flags in [('native',[]),('asan-ubsan',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'])]:
   for flow in [0,1]:
    exe=Path(td)/'test';cmd=['c++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-exceptions','-fno-rtti',*flags,'-DASYNCWEBSERVER_USE_CHUNK_INFLIGHT='+str(flow),*['-I'+str(x) for x in [P,P/'host',H,M,U]],P/'Test.cpp',U/'FS.cpp','-o',exe]
    t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,t.stderr
    t=subprocess.run([exe],capture_output=True,text=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'));r['profiles'][profile+'-flow'+str(flow)]={'stdout':t.stdout,'stderr':t.stderr,'returncode':t.returncode};save();assert t.returncode==0,(t.stdout,t.stderr);print(t.stdout)
 r['status']='PASS';save();print(out)
except Exception as e:r['status']='FAIL';r['error']=str(e);save();raise
