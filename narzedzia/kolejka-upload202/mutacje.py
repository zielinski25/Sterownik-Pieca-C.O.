from pathlib import Path
import subprocess,tempfile,json,hashlib,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent;J=N/'kontekst-upload201';H=N/'uchwyty-runtime197';M=N/'most-runtime198';Q=N/'upload-zadania200';U=N/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie';original=(P/'UploadPump.h').read_text()
mutants=[('lost-data-copy','if(f.length)std::memcpy(f.data,data+at,f.length);',''),('wrong-piece-offset','f.index=index+at;','f.index=index;'),('final-on-every-piece','f.final=final&&i+1==parts;','f.final=final;'),('lost-final','f.final=final&&i+1==parts;','f.final=false;'),('non-atomic-reservation','if(parts>Frames-count_)','if(parts>Frames)'),('lost-end','r->ended=true;',''),('success-on-rejection','r.failure=status;','status=200;r.failure=status;'),('lost-disconnect','r->connected=false;',''),('old-token-matches-new','a.slot==b.slot&&a.generation==b.generation','a.slot==b.slot'),('duplicate-attach','if(r.occupied&&r.key==key)','if(false)'),('second-worker','if(running_)return Step::Busy;',''),('duplicate-receipt','||r->delivered','')]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for n in ['Test.cpp','Fixture201.inc']:(d/n).write_bytes((P/n).read_bytes())
 for name,a,b in mutants:
  assert original.count(a)==1,(name,original.count(a));(d/'UploadPump.h').write_text(original.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-pthread','-fno-exceptions','-fno-rtti',*['-I'+str(x) for x in [d,J,Q,H,H/'host',M,U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=20);assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode})
inputs=[P/'UploadPump.h',P/'Test.cpp',P/'Fixture201.inc',P/'mutacje.py',J/'RequestRegistry.h',Q/'UploadPool.h',Q/'Raw198.inc',H/'RuntimeFilesystem.h',H/'host/Arduino.h',M/'Model197.inc',U/'FS.cpp',U/'FS.h',U/'FSImpl.h',B/'CheckedHistoryIO.h']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS queue202 compiled mutants',len(cases))
