from pathlib import Path
import tempfile,subprocess,json,hashlib,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent;H=N/'uchwyty-runtime197';M=N/'most-runtime198';Q=N/'upload-zadania200';U=N/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie';s=(P/'RequestRegistry.h').read_text()
mutations=[('stale-token','s.occupied&&s.generation==t.generation','s.occupied'),('duplicate-attach','if(s.occupied&&s.key==key)','if(false)'),('ignore-disconnect','if(!s||s->disconnected)','if(!s)'),('lose-disconnect-publication','s->disconnected=gone;','s->disconnected=false;'),('forget-quarantine','else s->retained=true;','else s->occupied=false;'),('name-mismatch','||std::memcmp(name,w.name,nameLength)',''),('missing-file-success','if(!w.seen&&!w.error)w.error=400;','if(!w.seen&&!w.error)w.error=200;'),('ignore-authorization','s.error=authorized?0:403;','s.error=0;'),('repeat-error-reply','if(!w.replied){','if(true){')]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
with tempfile.TemporaryDirectory() as td:
 d=Path(td);(d/'Test.cpp').write_bytes((P/'Test.cpp').read_bytes())
 for name,a,b in mutations:
  assert s.count(a)==1,(name,s.count(a));(d/'RequestRegistry.h').write_text(s.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-pthread','-fno-exceptions','-fno-rtti',*['-I'+str(x) for x in [d,Q,H,H/'host',M,U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test'];t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=20);assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode})
inputs=[P/'RequestRegistry.h',P/'Test.cpp',P/'mutacje.py',Q/'UploadPool.h',Q/'Raw198.inc',H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS registry201 compiled mutants',len(cases))
