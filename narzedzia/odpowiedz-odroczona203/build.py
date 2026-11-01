from pathlib import Path
import hashlib,json,difflib
P=Path(__file__).resolve().parent;N=P.parent;H=N/'uchwyty-runtime197';A=N/'odpowiedz-http199'
h=(H/'upstream/ESPAsyncWebServer.h').read_text();s=(A/'upstream/WebRequest.cpp').read_text()
assert hashlib.sha256((H/'upstream/ESPAsyncWebServer.h').read_bytes()).hexdigest()==json.loads((H/'upstream/HEADER_SOURCE.json').read_text())['sha256']
assert hashlib.sha256((A/'upstream/WebRequest.cpp').read_bytes()).hexdigest()==json.loads((A/'upstream/REQUEST_SOURCE.json').read_text())['sha256']
def function(text,sig):
 a=text.index(sig);i=text.index('{',a)+1;depth=1
 while depth:
  if text[i]=='{':depth+=1
  elif text[i]=='}':depth-=1
  i+=1
 return text[a:i]
fields='''
  void (*_deferredPoll203)(AsyncWebServerRequest*, void*) = nullptr;
  void (*_deferredDisconnect203)(void*) = nullptr;
  void* _deferredContext203 = nullptr;
  bool _disconnected203 = false;
  bool _pollBusy203 = false;
'''
api='''
  // Only from the request/network context. Does not replace AsyncClient callbacks.
  bool installDeferred203(void (*poll)(AsyncWebServerRequest*, void*), void (*gone)(void*), void* context) {
    if (!poll || !gone || _sent || _disconnected203 || _pollBusy203 || _deferredDisconnect203) return false;
    _deferredContext203 = context; _deferredDisconnect203 = gone; _deferredPoll203 = poll; return true;
  }
  void stopDeferredPoll203() { _deferredPoll203 = nullptr; }
  bool disconnected203() const { return _disconnected203; }
'''
needle='  std::shared_ptr<AsyncWebServerRequest> _this;';assert h.count(needle)==1
newh=h.replace(needle,needle+fields)
needle='  void onDisconnect(ArDisconnectHandler fn);';assert newh.count(needle)==1;newh=newh.replace(needle,needle+'\n#define HISTORY203_DEFERRED_POLL 1\n'+api)
oldpoll=function(s,'void AsyncWebServerRequest::_onPoll()')
poll='''void AsyncWebServerRequest::_onPoll() {
  std::shared_ptr<AsyncWebServerRequest> self = _this;
  if (!self || _disconnected203 || _pollBusy203) return;
  if (_deferredPoll203) {
    auto poll = _deferredPoll203; auto context = _deferredContext203;
    _pollBusy203 = true; poll(this, context); _pollBusy203 = false;
    if (_disconnected203 || !_client || !_this) return;
  }
  if (_response && _client && _client->canSend()) {
    _response->_ack(this, 0, 0);
  }
}'''
oldgone=function(s,'void AsyncWebServerRequest::_onDisconnect()')
gone='''void AsyncWebServerRequest::_onDisconnect() {
  if (_disconnected203) return;
  std::shared_ptr<AsyncWebServerRequest> self = _this;
  _disconnected203 = true;
  auto gone = _deferredDisconnect203; auto context = _deferredContext203;
  _deferredPoll203 = nullptr; _deferredDisconnect203 = nullptr; _deferredContext203 = nullptr;
  if (gone) gone(context);
  if (_onDisconnectfn) _onDisconnectfn();
  _this.reset();
}'''
news=s.replace(oldpoll,poll).replace(oldgone,gone)
c=P/'candidate';c.mkdir(exist_ok=True);(c/'ESPAsyncWebServer.h').write_text(newh);(c/'WebRequest.cpp').write_text(news)
for n in ['WebResponses.cpp','WebResponseImpl.h']:(c/n).write_bytes((A/'candidate'/n).read_bytes())
patch=(A/'candidate/library.patch').read_text()+''.join(difflib.unified_diff(h.splitlines(True),newh.splitlines(True),fromfile='a/src/ESPAsyncWebServer.h',tofile='b/src/ESPAsyncWebServer.h'))+''.join(difflib.unified_diff(s.splitlines(True),news.splitlines(True),fromfile='a/src/WebRequest.cpp',tofile='b/src/WebRequest.cpp'))
(c/'library.patch').write_text(patch)
(P/'Factory.inc').write_text('// Exact pinned request factory. LGPL-3.0-or-later.\n'+function(h,'  static std::shared_ptr<AsyncWebServerRequest> create(')+'\n')
(P/'Fields.inc').write_text('// LGPL-3.0-or-later, candidate additions.\n'+fields)
(P/'API.inc').write_text('// LGPL-3.0-or-later, candidate additions.\n#define HISTORY203_DEFERRED_POLL 1\n'+api)
methods=[poll,gone]
for sig in ['AsyncWebServerRequest::~AsyncWebServerRequest()', 'AsyncWebServerRequestPtr AsyncWebServerRequest::pause()', 'void AsyncWebServerRequest::abort()', 'void AsyncWebServerRequest::send(AsyncWebServerResponse *response)', 'void AsyncWebServerRequest::_send()', 'void AsyncWebServerRequest::onDisconnect(']:methods.append(function(s,sig))
(P/'Methods.inc').write_text('// LGPL-3.0-or-later: exact pinned methods, candidate poll/disconnect.\n'+'\n\n'.join(methods)+'\n')
(P/'EXTRACTION.json').write_text(json.dumps({'request_sha256':hashlib.sha256(s.encode()).hexdigest(),'header_sha256':hashlib.sha256(h.encode()).hexdigest(),'original_methods':6,'modified_methods':['poll','disconnect'],'combined_with_199':True,'full_library_compiled':False},indent=2)+'\n')
print('PASS 203 pinned sources, combined patch and exact method extraction')
