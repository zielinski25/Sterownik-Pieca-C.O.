from pathlib import Path
import tempfile,subprocess,json,hashlib,os,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent;H=N/'uchwyty-runtime197';M=N/'most-runtime198';Q=N/'upload-zadania200';J=N/'kontekst-upload201';K=N/'kolejka-upload202';U=N/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie'
D=N/'odpowiedz-odroczona203'
A=N/'adapter-upload204'
mutants=[('early-final', 'Parser205.inc', 'if (_strictMultipart205) return; // no final before the delimiter suffix is validated', ''), ('ignore-tail-byte', 'Parser205.inc', 'data != tail[_strictTail205++]', '(++_strictTail205 && false)'), ('rewrite-length', 'Parser205.inc', 'if (_parsedLength + 1 != _contentLength) {', '_contentLength = _parsedLength + 1;\n      if (_parsedLength + 1 != _contentLength) {'), ('ignore-completion', 'Parser205.inc', 'if (_strictMultipart205 && !_strictMultipartDone205) {', 'if (false) {'), ('continue-after-gone', 'Parser205.inc', 'if (_disconnected203 || _sent) return;', ''), ('allow-multipart-fields', 'Parser205.inc', 'if (_strictMultipart205 && (!_itemIsFile || !_itemName.length() || !_itemFilename.length())) {', 'if (false) {'), ('lost-route-end-gate', 'StrictMultipartEnd.h', 'if(!r.strictMultipartComplete205()){r.abort();return false;}', ''), ('no-file-length-check', 'UploadAdapter.h', 'auto token=pump_.attach(reinterpret_cast<uintptr_t>(&r),expected,true);', 'auto token=pump_.attach(reinterpret_cast<uintptr_t>(&r),expected+1,true);'), ('allow-chunked', 'Parser205.inc', '|| _chunkedParseState != CHUNK_NONE', ''), ('unbounded-body', 'Parser205.inc', '|| _contentLength > maxBody', ''), ('invalid-boundary', 'Parser205.inc', '|| _boundary.length() > 70', ''), ('missing-parse-error-abort', 'Parser205.inc', 'if (strictMultipartError205()) { _parseState = PARSE_REQ_FAIL; abort(); return; }', '')]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
files=['Test.cpp','Model203.inc','Parser205.inc','StrictMultipartEnd.h','ParserFields.inc','ParserModel.h','UploadAdapter.h'];original={n:((A if n=='UploadAdapter.h' else P)/n).read_text() for n in files}
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for name,file,a,b in mutants:
  for n,s in original.items():(d/n).write_text(s)
  s=original[file];assert s.count(a)==1,(name,s.count(a));(d/file).write_text(s.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-g','-pthread','-fno-exceptions','-fno-rtti','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',*['-I'+str(x) for x in [d,P/'host',P,A,D,K,J,Q,H,H/'host',M,U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'));assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode,'asan_error': 'ERROR: AddressSanitizer' in t.stderr})
inputs=[(A if n=='UploadAdapter.h' else P)/n for n in files]+[P/'mutacje.py',K/'UploadPump.h',K/'Fixture201.inc',J/'RequestRegistry.h',Q/'UploadPool.h',Q/'Raw198.inc',H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','profile':'ASan+UBSan, LSan enabled','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS parser205 compiled mutants',len(cases))
