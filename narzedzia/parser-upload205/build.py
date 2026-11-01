from pathlib import Path
import hashlib,json,difflib
P=Path(__file__).resolve().parent;N=P.parent;D=N/'odpowiedz-odroczona203';A=N/'odpowiedz-http199';H=N/'uchwyty-runtime197'
orig=(A/'upstream/WebRequest.cpp').read_text();assert hashlib.sha256(orig.encode()).hexdigest()==json.loads((A/'upstream/REQUEST_SOURCE.json').read_text())['sha256']
s=(D/'candidate/WebRequest.cpp').read_text();h=(D/'candidate/ESPAsyncWebServer.h').read_text()
def fn(t,sig):
 a=t.index(sig);b=t.index('\n}\n',a)+2;assert b>a;return t[a:b]
def change(text,a,b):
 assert text.count(a)==1,(a[:90],text.count(a));return text.replace(a,b)
fields='''
  bool _strictMultipart205 = false;
  bool _strictMultipartDone205 = false;
  uint8_t _strictTail205 = 0;
'''
api='''
  // Call in the serialized network context after headers, before any body byte.
  bool enableStrictMultipart205(size_t maxBody);
  bool strictMultipartComplete205() const;
'''
h=change(h,'  bool _pollBusy203 = false;','  bool _pollBusy203 = false;'+fields)
h=change(h,'  bool disconnected203() const { return _disconnected203; }','  bool disconnected203() const { return _disconnected203; }'+api)
methods='''bool AsyncWebServerRequest::enableStrictMultipart205(size_t maxBody) {
  if (_strictMultipart205 || _parsedLength || _sent || _paused || _disconnected203 || !_handler ||
      (_parseState != PARSE_REQ_HEADERS && _parseState != PARSE_REQ_BODY) ||
      !_isMultipart || _chunkedParseState != CHUNK_NONE || !_contentLength ||
      !maxBody || _contentLength > maxBody || !_boundary.length() || _boundary.length() > 70) return false;
  _strictMultipart205 = true;
  return true;
}

bool AsyncWebServerRequest::strictMultipartComplete205() const {
  return _strictMultipart205 && _strictMultipartDone205 && !_disconnected203 &&
         _parseState == PARSE_REQ_END && _parsedLength == _contentLength;
}'''
# Multipart enum precedes parser; add a state without renumbering original states.
s=change(s,'  PARSE_ERROR\n};','  PARSE_ERROR,\n  STRICT_CLOSING205\n};')
# Original final notification block, executed unchanged only after strict trailer.
a=s.index('      if (!_itemIsFile) {',s.index('void AsyncWebServerRequest::_parseMultipartPostByte'))
b=s.index('\n\n    } else {',a)
finish=s[a:b]
strict='''  if (_strictMultipart205 && _multiParseState == STRICT_CLOSING205) {
    static const uint8_t tail[] = {'-', '\\r', '\\n'};
    if (_strictTail205 >= sizeof(tail) || data != tail[_strictTail205++]) {
      _multiParseState = PARSE_ERROR;
      return;
    }
    if (_strictTail205 == sizeof(tail)) {
      if (_parsedLength + 1 != _contentLength) {
        _multiParseState = PARSE_ERROR;
        return;
      }
      _multiParseState = PARSING_FINISHED;
      _strictMultipartDone205 = true;
'''+finish+'''
    }
    return;
  }
'''
s=change(s,'  if (_multiParseState == WAIT_FOR_RETURN1) {',strict+'\n  if (_multiParseState == WAIT_FOR_RETURN1) {')
s=change(s,'        // value starts from here','''        if (_strictMultipart205 && (!_itemIsFile || !_itemName.length() || !_itemFilename.length())) {
          _multiParseState = PARSE_ERROR;
          return;
        }
        // value starts from here''')
s=change(s,'      _multiParseState = DASH3_OR_RETURN2;','''      _multiParseState = DASH3_OR_RETURN2;
      if (_strictMultipart205) return; // no final before the delimiter suffix is validated''')
s=change(s,'  } else if (_multiParseState == DASH3_OR_RETURN2) {','''  } else if (_multiParseState == DASH3_OR_RETURN2) {
    if (_strictMultipart205) {
      if (data != '-') { _multiParseState = PARSE_ERROR; return; }
      _strictTail205 = 0;
      _multiParseState = STRICT_CLOSING205;
      return;
    }''')
# _onData is above the multipart enum: numeric values must not leak into the guard.
# Add private enum-independent error query to API, implemented after multipart enum.
h=change(h,'  bool strictMultipartComplete205() const;','  bool strictMultipartComplete205() const;\n  bool strictMultipartError205() const;')
methods+='''

bool AsyncWebServerRequest::strictMultipartError205() const {
  return _strictMultipart205 && _multiParseState == PARSE_ERROR;
}'''
s=change(s,'            _parseMultipartPostByte(((uint8_t *)buf)[i], i == len - 1);\n            _parsedLength++;','''            _parseMultipartPostByte(((uint8_t *)buf)[i], i == len - 1);
            _parsedLength++;
            if (_strictMultipart205) {
              if (strictMultipartError205()) { _parseState = PARSE_REQ_FAIL; abort(); return; }
              if (_disconnected203 || _sent) return;
            }''')
s=change(s,'      if (_parsedLength == _contentLength) {','''      if (_parsedLength == _contentLength) {
        if (_strictMultipart205 && !_strictMultipartDone205) {
          _parseState = PARSE_REQ_FAIL; abort(); return;
        }''')
# Incoming callbacks after a strict request disconnect must not resume parsing.
s=change(s,'  // SSL/TLS handshake detection','''  if (_strictMultipart205 && (_disconnected203 || _sent || !len)) return;

  // SSL/TLS handshake detection''')
needle='void AsyncWebServerRequest::_parseLine()';s=change(s,needle,methods+'\n\n'+needle)
c=P/'candidate';c.mkdir(exist_ok=True);(c/'WebRequest.cpp').write_text(s);(c/'ESPAsyncWebServer.h').write_text(h)
for n in ['WebResponses.cpp','WebResponseImpl.h']:(c/n).write_bytes((D/'candidate'/n).read_bytes())
patch=(D/'candidate/library.patch').read_text()+''.join(difflib.unified_diff((D/'candidate/ESPAsyncWebServer.h').read_text().splitlines(True),h.splitlines(True),fromfile='a/src/ESPAsyncWebServer.h',tofile='b/src/ESPAsyncWebServer.h'))+''.join(difflib.unified_diff((D/'candidate/WebRequest.cpp').read_text().splitlines(True),s.splitlines(True),fromfile='a/src/WebRequest.cpp',tofile='b/src/WebRequest.cpp'))
(c/'library.patch').write_text(patch)
(P/'Fields205.inc').write_text(fields)
(P/'API205.inc').write_text(api+'  bool strictMultipartError205() const;\n')
enums=orig[orig.index('enum {'):orig.index('AsyncWebServerRequest::AsyncWebServerRequest')]
(P/'Enums.inc').write_text(enums)
mp_start=s.index('enum {\n  EXPECT_BOUNDARY');mp_end=s.index('\nvoid AsyncWebServerRequest::_parseLine()',mp_start)
(P/'Parser205.inc').write_text('// LGPL-3.0-or-later, extracted candidate bodies.\n'+fn(s,'void AsyncWebServerRequest::_onData(')+'\n'+fn(s,'void AsyncWebServerRequest::_handleUploadByte(')+'\n'+s[mp_start:mp_end])
a=orig.index('enum {\n  EXPECT_BOUNDARY');b=orig.index('\nvoid AsyncWebServerRequest::_parseLine()',a)
(P/'ParserOriginal.inc').write_text('// LGPL-3.0-or-later, exact pinned parser bodies.\n'+fn(orig,'void AsyncWebServerRequest::_onData(')+'\n'+fn(orig,'void AsyncWebServerRequest::_handleUploadByte(')+'\n'+orig[a:b])
(P/'EXTRACTION.json').write_text(json.dumps({'release_ready':False,'request_sha256':hashlib.sha256(orig.encode()).hexdigest(),'original_methods':['_onData','_handleUploadByte','_parseMultipartPostByte'],'candidate_methods':['strict multipart framing','strict-mode _onData checks'],'full_headers_parser_and_library_compiled':False,'String':'host model, not Arduino implementation'},indent=2)+'\n')
# Apply the cumulative patch to a clean pinned tree and require byte equality.
import tempfile,shutil,subprocess
with tempfile.TemporaryDirectory() as td:
 t=Path(td)/'src';t.mkdir()
 shutil.copy(A/'upstream/WebRequest.cpp',t/'WebRequest.cpp')
 shutil.copy(H/'upstream/ESPAsyncWebServer.h',t/'ESPAsyncWebServer.h')
 shutil.copy(A/'upstream/WebResponseImpl.h',t/'WebResponseImpl.h')
 shutil.copy(H/'upstream/WebResponses.cpp',t/'WebResponses.cpp')
 run=subprocess.run(['patch','-p1','--batch','--forward'],input=(c/'library.patch').read_text(),text=True,cwd=td,capture_output=True)
 assert run.returncode==0,run.stdout+run.stderr
 for n in ['WebRequest.cpp','ESPAsyncWebServer.h','WebResponses.cpp','WebResponseImpl.h']:
  assert (t/n).read_bytes()==(c/n).read_bytes(),n
print('PASS pinned parser extraction and cumulative candidate205 patch')
