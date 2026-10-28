from pathlib import Path
import subprocess,tempfile,json,hashlib,resource
P=Path(__file__).resolve().parent;R=P.parents[1];H=P.parent/'uchwyty-runtime197';M=P.parent/'most-runtime198';U=P.parent/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie';original=(P/'UploadPool.h').read_text()
mutants=[
 ('stale-token','s.occupied&&s.generation==t.generation','s.occupied'),
 ('duplicate-success','if(s.emitted)return {false,s.status,s.verified};',''),
 ('accept-without-final','else if(s.phase!=FinalSeen)reject(s,400);',''),
 ('ignore-readback-corruption','if(std::memcmp(verify,data+at,count))return failIO(s);',''),
 ('omit-per-block-sync','||!sync(s)||!seek','||!seek'),
 ('omit-final-sync','else if(sync(s)){','else {'),
 ('skip-close','const int r=raw_.close(s.fd);','const int r=0;'),
 ('close-after-stop','if(s.descriptor!=Open||!active(s))return false;','if(s.descriptor!=Open)return false;'),
 ('forget-quarantine','if(s.descriptor!=None)return false;',''),
 ('allow-existing-file','O_RDWR|O_CREAT|O_EXCL','O_RDWR|O_CREAT'),
 ('ignore-read-count','check(s,read==static_cast<ssize_t>(count))','check(s,true)'),
 ('ignore-write-count','check(s,got==static_cast<ssize_t>(count))','check(s,true)'),
 ('ignore-stop-after-io','&&owner_.ready())||failIO(s)',' )||failIO(s)'),
 ('release-before-complete','s.phase=FinalSeen;','s.phase=Confirmed;s.status=200;'),
 ('missing-offset-check','index!=s.verified||',''),
]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for n in ['Test.cpp','Raw198.inc']:(d/n).write_bytes((P/n).read_bytes())
 for name,a,b in mutants:
  assert original.count(a)==1,(name,original.count(a));(d/'UploadPool.h').write_text(original.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-pthread','-fno-exceptions','-fno-rtti',*['-I'+str(x) for x in [d,H,H/'host',M,U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=20);assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode})
inputs=[P/'UploadPool.h',P/'Test.cpp',P/'Raw198.inc',P/'mutacje.py',H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp',B/'CheckedHistoryIO.h']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS compiled upload200 mutants',len(cases))
