from pathlib import Path
import tempfile,subprocess,json,hashlib,os,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent;H=N/'uchwyty-runtime197';M=N/'most-runtime198';Q=N/'upload-zadania200';J=N/'kontekst-upload201';K=N/'kolejka-upload202';U=N/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie'
D=N/'odpowiedz-odroczona203'
mutants=[('reuse-live-request', 'UploadAdapter.h', 's.disconnected&&s.request.expired()', 's.disconnected'), ('stale-generation', 'UploadAdapter.h', '||s.generation!=h.generation', ''), ('foreign-request', 'UploadAdapter.h', 'live&&live.get()==&r?&s:nullptr', 'live?&s:nullptr'), ('lost-cancellation', 'UploadAdapter.h', 's.owner->pump_.disconnect(s.token);', ''), ('failed-bind-leaks-token', 'UploadAdapter.h', 'pump_.disconnect(token);return {Code::Conflict', 'return {Code::Conflict'), ('no-timeout', 'UploadAdapter.h', '>=s.timeout', '>s.timeout'), ('ignore-factory-race', 'UploadAdapter.h', 'if(s.done||r.disconnected203()||r.isSent()||r.getResponse()){stop(s,r);return;}', ''), ('wrong-success', 'UploadAdapter.h', 'services_.makeResponse(reply.status)', 'services_.makeResponse(200)'), ('no-authorization', 'UploadAdapter.h', 'if(!authorized)return', 'if(authorized&&false)return'), ('no-generation-increment', 'UploadAdapter.h', '++s.generation;', 's.generation=1;'), ('ignore-writable', 'UploadAdapter.h', '||!r.client()->canSend()', ''), ('pause-before-end', 'UploadAdapter.h', 's.occupied=true;s.disconnected=false;s.waiting=false;s.done=false;', 's.occupied=true;s.disconnected=false;s.waiting=false;s.done=false;r.pause();')]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
files=['Test.cpp','Fixture203.inc','UploadAdapter.h'];original={n:(P/n).read_text() for n in files}
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for name,file,a,b in mutants:
  for n,s in original.items():(d/n).write_text(s)
  s=original[file];assert s.count(a)==1,(name,s.count(a));(d/file).write_text(s.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-g','-pthread','-fno-exceptions','-fno-rtti','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',*['-I'+str(x) for x in [d,D,K,J,Q,H,H/'host',M,U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'));assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode,'asan_error': 'ERROR: AddressSanitizer' in t.stderr})
inputs=[P/n for n in files]+[P/'mutacje.py',K/'UploadPump.h',K/'Fixture201.inc',J/'RequestRegistry.h',Q/'UploadPool.h',Q/'Raw198.inc',H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','profile':'ASan+UBSan, LSan enabled','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS adapter204 compiled mutants',len(cases))
