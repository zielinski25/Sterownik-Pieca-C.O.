from pathlib import Path
import hashlib,json,difflib,re
P=Path(__file__).resolve().parent;H=P.parent/'uchwyty-runtime197';S=H/'upstream/WebResponses.cpp';s=S.read_text()
assert hashlib.sha256(S.read_bytes()).hexdigest()==json.loads((H/'upstream/SOURCES.json').read_text())['sha256']
server=(H/'upstream/ESPAsyncWebServer.h').read_bytes();assert hashlib.sha256(server).hexdigest()==json.loads((H/'upstream/HEADER_SOURCE.json').read_text())['sha256'];assert re.search(r'#define RESPONSE_TRY_AGAIN\s+0xFFFFFFFF',server.decode())
for n,m in [('WebResponseImpl.h','SOURCE.json'),('WebRequest.cpp','REQUEST_SOURCE.json')]:assert hashlib.sha256((P/'upstream'/n).read_bytes()).hexdigest()==json.loads((P/'upstream'/m).read_text())['sha256']
def function(s,signature):
 a=s.index(signature);i=s.index('{',a)+1;depth=1
 while depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 return s[a:i]
body=function(s,'size_t AsyncAbstractResponse::write_send_buffs(');original=body
abort='''if (!_sourceValid()) {
        _state = RESPONSE_FAILED;
        request->client()->close();
        return 0;
      }'''
changes=[('if (_state == RESPONSE_CONTENT) {','if (_state == RESPONSE_CONTENT || (_state == RESPONSE_END && _send_buffer_len)) {'),
 ('    do {\n','    do {\n      '+abort+'\n'),
 ('        if (added_len != _send_buffer_len - _send_buffer_offset) {','        payloadlen += added_len;\n        if (added_len != _send_buffer_len - _send_buffer_offset) {'),
 ('        payloadlen += added_len;\n      }','      }'),
 ('        if (readLen != RESPONSE_TRY_AGAIN) {','''        if (!_sourceValid() || (readLen != RESPONSE_TRY_AGAIN && readLen > std::min(_send_buffer->size(), tcp_win) - 8)) {
          _state = RESPONSE_FAILED; request->client()->close(); return 0;
        }
        if (readLen != RESPONSE_TRY_AGAIN) {'''),
 ('        if (readLen == 0) {','''        if (!_sourceValid() || (readLen != RESPONSE_TRY_AGAIN && readLen > maxLen)) {
          _state = RESPONSE_FAILED; request->client()->close(); return 0;
        }
        if (readLen == 0) {'''),
 ('    // execute sending whatever','    '+abort+'\n    // execute sending whatever')]
for a,b in changes:assert body.count(a)==1,a;body=body.replace(a,b)
patched=s.replace(original,body)
h=(P/'upstream/WebResponseImpl.h').read_text();needle='  virtual ~AsyncAbstractResponse() {}';replacement=needle+'''
  bool _finished() const override {
    return _state == RESPONSE_FAILED || (_state > RESPONSE_WAIT_ACK && _send_buffer_len == 0);
  }''';assert h.count(needle)==1;newh=h.replace(needle,replacement).replace('class AsyncAbstractResponse :', '#define HISTORY199_RESPONSE_GUARD 1\nclass AsyncAbstractResponse :')
(P/'candidate').mkdir(exist_ok=True)
(P/'candidate/WebResponses.cpp').write_text(patched);(P/'candidate/WebResponseImpl.h').write_text(newh)
patch=''.join(difflib.unified_diff(s.splitlines(True),patched.splitlines(True),fromfile='a/src/WebResponses.cpp',tofile='b/src/WebResponses.cpp'))+''.join(difflib.unified_diff(h.splitlines(True),newh.splitlines(True),fromfile='a/src/WebResponseImpl.h',tofile='b/src/WebResponseImpl.h'))
(P/'candidate/library.patch').write_text(patch)
a=newh.index('#define HISTORY199_RESPONSE_GUARD 1');b=newh.index('\n#ifndef TEMPLATE_PLACEHOLDER',a)
(P/'Abstract.inc').write_text('// LGPL-3.0-or-later: pinned declaration with candidate finished guard.\n'+newh[a:b])
methods=[function(s,'AsyncAbstractResponse::AsyncAbstractResponse('),body,function(s,'bool AsyncWebServerResponse::_finished() const')]
r=(P/'upstream/WebRequest.cpp').read_text()
for sig in ['void AsyncWebServerRequest::_onAck(', 'void AsyncWebServerRequest::_onPoll(', 'void AsyncWebServerRequest::_onTimeout(']:methods.append(function(r,sig))
(P/'Transport.inc').write_text('// LGPL-3.0-or-later: exact pinned methods, candidate send loop.\n'+'\n\n'.join(methods)+'\n')
(P/'EXTRACTION.json').write_text(json.dumps({'original_send_sha256':hashlib.sha256(original.encode()).hexdigest(),'candidate_send_sha256':hashlib.sha256(body.encode()).hexdigest(),'edits':len(changes)+2,'templates_qualified':False,'full_library_compiled':False},indent=2)+'\n')
print('PASS pinned source verification and deterministic library patch')
