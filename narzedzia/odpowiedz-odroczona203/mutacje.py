from pathlib import Path
import tempfile,subprocess,json,hashlib,os,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent;H=N/'uchwyty-runtime197';M=N/'most-runtime198';Q=N/'upload-zadania200';J=N/'kontekst-upload201';K=N/'kolejka-upload202';U=N/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie'
mutants=[
 ('reuse-before-request-destruction','DeferredUploadReply.h','||!request_.expired()',''),
 ('no-poll-lifetime-pin','Methods.inc','std::shared_ptr<AsyncWebServerRequest> self = _this;\n  if (!self','const bool self = bool(_this);\n  if (!self'),
 ('no-reentrancy-guard','Methods.inc','if (!self || _disconnected203 || _pollBusy203)','if (!self || _disconnected203)'),
 ('continue-after-disconnect','Methods.inc','if (_disconnected203 || !_client || !_this) return;',''),
 ('lost-disconnect-notification','Methods.inc','if (gone) gone(context);',''),
 ('break-existing-callback-lifetime','Methods.inc','if (_onDisconnectfn) _onDisconnectfn();\n  _this.reset();','_this.reset();\n  if (_onDisconnectfn) _onDisconnectfn();'),
 ('poll-only-when-writable','Methods.inc','if (_deferredPoll203) {','if (_deferredPoll203 && _client->canSend()) {'),
 ('ignore-factory-race','DeferredUploadReply.h','if(done_||r.disconnected203()||r.isSent()||r.getResponse()){stop(r);return;}',''),
 ('false-status-success','DeferredUploadReply.h','services_.makeResponse(reply.status)','services_.makeResponse(200)'),
 ('ignore-foreign-response','DeferredUploadReply.h','if(r.isSent()||r.getResponse()){stop(r);return;}',''),
 ('ignore-oom','DeferredUploadReply.h','if(!response){stop(r);r.abort();return;}',''),
 ('hook-overwrite','API.inc','|| _deferredDisconnect203',''),
 ('ignore-prior-pause','DeferredUploadReply.h','||r.isPaused()',''),
]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
files=['Test.cpp','Fixture202.inc','Fields.inc','Factory.inc','API.inc','Methods.inc','DeferredUploadReply.h'];original={n:(P/n).read_text() for n in files}
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for name,file,a,b in mutants:
  for n,s in original.items():(d/n).write_text(s)
  s=original[file];assert s.count(a)==1,(name,s.count(a));(d/file).write_text(s.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-g','-pthread','-fno-exceptions','-fno-rtti','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',*['-I'+str(x) for x in [d,K,J,Q,H,H/'host',M,U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'));assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode,'asan_error': 'ERROR: AddressSanitizer' in t.stderr})
inputs=[P/n for n in files]+[P/'mutacje.py',K/'UploadPump.h',K/'Fixture201.inc',J/'RequestRegistry.h',Q/'UploadPool.h',Q/'Raw198.inc',H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','profile':'ASan+UBSan, LSan enabled','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS deferred203 compiled mutants',len(cases))
