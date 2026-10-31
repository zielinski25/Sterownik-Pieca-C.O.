from pathlib import Path
import tempfile,subprocess,json,hashlib,os,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent;H=N/'uchwyty-runtime197';M=N/'most-runtime198';Q=N/'upload-zadania200';J=N/'kontekst-upload201';K=N/'kolejka-upload202';U=N/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie'
D=N/'odpowiedz-odroczona203'
A=N/'adapter-upload204'
X=N/'parser-upload205'
mutants=[('allow-wrong-token', 'Test.cpp', 'if(r->authType()!=AUTH_BEARER||r->authChallenge()!=h.token.c_str()){', 'if(false){'), ('allow-noncanonical-length', 'Test.cpp', 'if(!canonical(r->header("Content-Length"),r->contentLength())||', 'if(!r->contentLength()||'), ('accept-other-route', 'Test.cpp', 'if(r->url()!="/upload"||r->method()!=AsyncWebRequestMethod::HTTP_POST)return true;', 'if(false)return true;'), ('admit-get', 'Test.cpp', 'if(r->url()!="/upload"||r->method()!=AsyncWebRequestMethod::HTTP_POST)return true;', 'if(r->url()!="/upload")return true;'), ('skip-strict-enable', 'Test.cpp', 'if(!r->enableStrictMultipart205(8192)){++h.framing;return false;}', ''), ('framing-not-fatal', 'Test.cpp', 'r->contentLength()>8192){++h.framing;return false;}', 'r->contentLength()>8192){++h.framing;return true;}'), ('refused-not-trivial', 'Test.cpp', 'bool isRequestHandlerTrivial()override{return rejecting;}', 'bool isRequestHandlerTrivial()override{return false;}'), ('replace-policy', 'Methods206.inc', 'if (!policy || _attachPolicy206) {', 'if (!policy) {'), ('skip-policy-call', 'Methods206.inc', 'if (!_server->_runAttachPolicy206(this)) {', 'if (false) {'), ('early-file-final', 'Parser206.inc', 'if (_strictMultipart205) return; // no final before the delimiter suffix is validated', ''), ('length-rewrite', 'Parser206.inc', 'if (_parsedLength + 1 != _contentLength) {', '_contentLength = _parsedLength + 1;\n      if (_parsedLength + 1 != _contentLength) {')]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
files=['Test.cpp','Fixture206.inc','Methods206.inc','Parser206.inc','AsyncWebHeader.inc','UploadAdapter.h'];original={n:((A if n=='UploadAdapter.h' else P)/n).read_text() for n in files}
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for name,file,a,b in mutants:
  for n,s in original.items():(d/n).write_text(s)
  s=original[file];assert s.count(a)==1,(name,s.count(a));(d/file).write_text(s.replace(a,b))
  cmd=['c++','-std=c++17','-O1','-g','-pthread','-fno-exceptions','-fno-rtti','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',*['-I'+str(x) for x in [d,P/'host',P,P/'upstream',X,A,D,K,J,Q,H,H/'host',M,U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'));assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode,'asan_error': 'ERROR: AddressSanitizer' in t.stderr})
inputs=[(A if n=='UploadAdapter.h' else P)/n for n in files]+[P/'mutacje.py',X/'StrictMultipartEnd.h',D/'Fields.inc',P/'upstream/literals.h',P/'upstream/AsyncWebHeader.cpp',D/'Fixture202.inc',K/'UploadPump.h',K/'Fixture201.inc',J/'RequestRegistry.h',Q/'UploadPool.h',Q/'Raw198.inc',H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','profile':'ASan+UBSan, LSan enabled','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS headers206 compiled mutants',len(cases))
