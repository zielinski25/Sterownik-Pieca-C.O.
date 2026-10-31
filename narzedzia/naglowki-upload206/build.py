from pathlib import Path
import hashlib,json,difflib,subprocess,shutil,tempfile
P=Path(__file__).resolve().parent;N=P.parent;A=N/'odpowiedz-http199';D=N/'odpowiedz-odroczona203';X=N/'parser-upload205';H=N/'uchwyty-runtime197'
sha=lambda b:hashlib.sha256(b).hexdigest()
# 1. Pinned sources. WebServer.cpp is new here; its hash is recorded from the GitHub API blob.
src=P/'upstream'
meta=json.loads((src/'SOURCES.json').read_text())
for name in ['literals.h','WebServer.cpp','AsyncWebHeader.cpp']:
 assert sha((src/name).read_bytes())==meta[name]['sha256'],name
orig_req=(A/'upstream/WebRequest.cpp').read_text();assert sha(orig_req.encode())==json.loads((A/'upstream/REQUEST_SOURCE.json').read_text())['sha256']
orig_h=H/'upstream/ESPAsyncWebServer.h'
assert sha(orig_h.read_bytes())==json.loads((H/'upstream/HEADER_SOURCE.json').read_text())['sha256']
orig_server=(src/'WebServer.cpp').read_text()
# 2. Regenerate 205 candidate so this stage cannot run on stale derived files.
subprocess.run(['python3','-B',str(X/'build.py')],check=True,capture_output=True)
req=(X/'candidate/WebRequest.cpp').read_text();hdr=(X/'candidate/ESPAsyncWebServer.h').read_text()
def fn(t,sig):
 a=t.index(sig);b=t.index('\n}\n',a)+2;assert b>a;return t[a:b]
def change(text,a,b):
 assert text.count(a)==1,(a[:80],text.count(a));return text.replace(a,b)
# 3. Candidate library addition: one server-level policy call after handler selection.
hdr=change(hdr,'  AsyncCallbackWebHandler *_catchAllHandler;','''  AsyncCallbackWebHandler *_catchAllHandler;
  AttachPolicy206 _attachPolicy206 = nullptr;
  void *_attachPolicyContext206 = nullptr;''')
hdr=change(hdr,'  void _attachHandler(AsyncWebServerRequest *request);','''  // Optional upload policy. It runs on the request context after handler
  // selection and before the first body byte / 100-continue. False aborts.
  typedef bool (*AttachPolicy206)(AsyncWebServerRequest *, void *);
  bool setAttachPolicy206(AttachPolicy206 policy, void *context);
  void clearAttachPolicy206();
  bool _runAttachPolicy206(AsyncWebServerRequest *request);
  void _attachHandler(AsyncWebServerRequest *request);''')
req=change(req,'      _server->_attachHandler(this);','''      _server->_attachHandler(this);
      if (!_server->_runAttachPolicy206(this)) {
        _parseState = PARSE_REQ_FAIL;
        abort();
        return;
      }''')
srv=orig_server
srv=change(srv,'void AsyncWebServer::_attachHandler(AsyncWebServerRequest *request) {','''bool AsyncWebServer::setAttachPolicy206(AttachPolicy206 policy, void *context) {
  if (!policy || _attachPolicy206) {
    return false;
  }
  _attachPolicy206 = policy;
  _attachPolicyContext206 = context;
  return true;
}

void AsyncWebServer::clearAttachPolicy206() {
  _attachPolicy206 = nullptr;
  _attachPolicyContext206 = nullptr;
}

bool AsyncWebServer::_runAttachPolicy206(AsyncWebServerRequest *request) {
  AttachPolicy206 policy = _attachPolicy206;
  return !policy || policy(request, _attachPolicyContext206);
}

void AsyncWebServer::_attachHandler(AsyncWebServerRequest *request) {''')
c=P/'candidate';c.mkdir(exist_ok=True)
(c/'ESPAsyncWebServer.h').write_text(hdr);(c/'WebRequest.cpp').write_text(req);(c/'WebServer.cpp').write_text(srv)
for n in ['WebResponses.cpp','WebResponseImpl.h']:(c/n).write_bytes((X/'candidate'/n).read_bytes())
base=(X/'candidate/library.patch').read_text()
patch=base+''.join(difflib.unified_diff((X/'candidate/ESPAsyncWebServer.h').read_text().splitlines(True),hdr.splitlines(True),fromfile='a/src/ESPAsyncWebServer.h',tofile='b/src/ESPAsyncWebServer.h'))
patch+=''.join(difflib.unified_diff((X/'candidate/WebRequest.cpp').read_text().splitlines(True),req.splitlines(True),fromfile='a/src/WebRequest.cpp',tofile='b/src/WebRequest.cpp'))
patch+=''.join(difflib.unified_diff(orig_server.splitlines(True),srv.splitlines(True),fromfile='a/src/WebServer.cpp',tofile='b/src/WebServer.cpp'))
(c/'library.patch').write_text(patch)
# 4. Apply the cumulative patch to a clean pinned tree and require byte equality.
with tempfile.TemporaryDirectory() as td:
 t=Path(td)/'src';t.mkdir()
 for name,text in [('WebRequest.cpp',orig_req),('ESPAsyncWebServer.h',orig_h.read_text()),('WebServer.cpp',orig_server),
                   ('WebResponseImpl.h',(A/'upstream/WebResponseImpl.h').read_text()),('WebResponses.cpp',(H/'upstream/WebResponses.cpp').read_text())]:
  (t/name).write_text(text)
 run=subprocess.run(['patch','-p1','--batch','--forward'],input=patch,text=True,cwd=td,capture_output=True)
 assert run.returncode==0,run.stdout+run.stderr
 for name in ['WebRequest.cpp','ESPAsyncWebServer.h','WebServer.cpp','WebResponseImpl.h','WebResponses.cpp']:
  assert (t/name).read_bytes()==(c/name).read_bytes(),name
# 5. Extract exact bodies. Header parsing and accessors must be unchanged from pinned.
headers=['void AsyncWebServerRequest::_addGetParams(','bool AsyncWebServerRequest::_parseReqHead()','String AsyncWebServerRequest::urlDecode(','bool AsyncWebServerRequest::_parseReqHeader()','size_t AsyncWebServerRequest::headers() const','const AsyncWebHeader *AsyncWebServerRequest::getHeader(const char *name) const','const AsyncWebHeader *AsyncWebServerRequest::getHeader(size_t num) const','const String &AsyncWebServerRequest::header(const char *name) const','const String &AsyncWebServerRequest::header(size_t i) const','const String &AsyncWebServerRequest::headerName(size_t i) const']
assert fn(req,'void AsyncWebServerRequest::_parseLine()')==fn(orig_req,'void AsyncWebServerRequest::_parseLine()').replace('      _server->_attachHandler(this);','''      _server->_attachHandler(this);
      if (!_server->_runAttachPolicy206(this)) {
        _parseState = PARSE_REQ_FAIL;
        abort();
        return;
      }''')
for sig in headers:
 assert fn(orig_req,sig)==fn(req,sig),sig
impl='''// LGPL-3.0-or-later: exact pinned request/header bodies plus candidate 205/206 additions.
'''+req[req.index('static inline const char *skip_ows('):req.index('bool AsyncWebServerRequest::_parseReqHeader()')]+'\n'.join(fn(req,s) for s in headers)
impl+='\nnamespace asyncsrv {\n'+fn(req,'WebRequestMethod stringToMethod(')+'\n}\n'
impl+='\n'+fn(req,'void AsyncWebServerRequest::_parseLine()')+'\n'
impl+=fn(srv,'bool AsyncWebServer::setAttachPolicy206(')+'\n'+fn(srv,'void AsyncWebServer::clearAttachPolicy206()')+'\n'+fn(srv,'bool AsyncWebServer::_runAttachPolicy206(')
(P/'Methods206.inc').write_text(impl)
(P/'AsyncWebHeader.inc').write_text('// LGPL-3.0-or-later: exact pinned AsyncWebHeader::parse.\n'+fn((src/'AsyncWebHeader.cpp').read_text(),'const AsyncWebHeader AsyncWebHeader::parse(const char *data)'))
a=req.index('enum {\n  EXPECT_BOUNDARY');b=req.index('\nvoid AsyncWebServerRequest::_parseLine()',a)
body='// LGPL-3.0-or-later: exact candidate 205/206 body parser and strict frames.\n'+fn(req,'void AsyncWebServerRequest::_onData(')+'\n'+fn(req,'void AsyncWebServerRequest::_handleUploadByte(')+'\n'+req[a:b]
assert body==('// LGPL-3.0-or-later: exact candidate 205/206 body parser and strict frames.\n'+fn((X/'candidate/WebRequest.cpp').read_text(),'void AsyncWebServerRequest::_onData(')+'\n'+fn((X/'candidate/WebRequest.cpp').read_text(),'void AsyncWebServerRequest::_handleUploadByte(')+'\n'+((X/'candidate/WebRequest.cpp').read_text())[(X/'candidate/WebRequest.cpp').read_text().index('enum {\n  EXPECT_BOUNDARY'):(X/'candidate/WebRequest.cpp').read_text().index('\nvoid AsyncWebServerRequest::_parseLine()',(X/'candidate/WebRequest.cpp').read_text().index('enum {\n  EXPECT_BOUNDARY'))])
(P/'Parser206.inc').write_text(body)
head=orig_h.read_text()
enums=orig_req[orig_req.index('enum {\n  PARSE_REQ_START'):orig_req.index('};',orig_req.index('  PARSE_REQ_FAIL = 4'))+2]
enums+='\n\n'+orig_req[orig_req.index('enum {\n  CHUNK_NONE'):orig_req.index('};',orig_req.index('  CHUNK_END,'))+2]
enums+='\n\n'+head[head.index('namespace AsyncWebRequestMethod {'):head.index('typedef AsyncWebRequestMethod::AsyncWebRequestMethodType WebRequestMethod;')+len('typedef AsyncWebRequestMethod::AsyncWebRequestMethodType WebRequestMethod;')]
enums+='\n\n'+head[head.index('typedef enum {\n  RCT_HTTP'):head.index('} RequestedConnectionType;')+len('} RequestedConnectionType;')]
enums+='\n\n'+head[head.index('typedef enum {\n  AUTH_NONE'):head.index('} AsyncAuthType;')+len('} AsyncAuthType;')]
(P/'Enums206.inc').write_text(enums+'\n')
(P/'EXTRACTION.json').write_text(json.dumps({'release_ready':False,'request_sha256':sha(orig_req.encode()),'header_sha256':sha(orig_h.read_bytes()),'server_sha256':sha(orig_server.encode()),'asyncwebheader_sha256':meta['AsyncWebHeader.cpp']['sha256'],'literals_sha256':meta['literals.h']['sha256'],
 'real_methods':['_parseReqHead','_addGetParams','urlDecode','_parseReqHeader','_parseLine','header','headerName','getHeader','headers','_onData','_handleUploadByte','_parseMultipartPostByte','strict frames','AsyncWebHeader::parse','stringToMethod'],
 'candidate_methods':['setAttachPolicy206','clearAttachPolicy206','_runAttachPolicy206'],'modeled':['String/Stream','TCP client and response','route selection','FreeRTOS scheduling','full library build'],'full_library_compiled':False},indent=2)+'\n')
print('PASS pinned header parser, cumulative 199+203+205+206 patch and exact extraction')
