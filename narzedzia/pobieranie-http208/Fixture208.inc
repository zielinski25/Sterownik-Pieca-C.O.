// Modeled TCP client and HTTP header bytes; real request methods from 203 and the
// real 199 send loop. The 197 owner and Arduino FS.cpp are real.
#include "Model197.inc"
#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#ifndef ASYNCWEBSERVER_USE_CHUNK_INFLIGHT
#define ASYNCWEBSERVER_USE_CHUNK_INFLIGHT 0
#endif
#define CONFIG_LWIP_TCP_MSS 32
#define CONFIG_LWIP_TCP_WND_DEFAULT 64
#define ASYNC_RESPONCE_BUFF_SIZE (CONFIG_LWIP_TCP_MSS * 2)
#define RESPONSE_TRY_AGAIN 0xFFFFFFFF
#define T_text_plain "text/plain"
#define async_ws_log_v(...) ((void)0)
using AwsTemplateProcessor = std::function<String(const String &)>;
using ArDisconnectHandler = std::function<void()>;
enum WebResponseState { RESPONSE_SETUP, RESPONSE_HEADERS, RESPONSE_CONTENT, RESPONSE_WAIT_ACK, RESPONSE_END, RESPONSE_FAILED };

struct Client {
 std::size_t window = 4096, limit = 4096, addCalls = 0, sends = 0, closes = 0, aborts = 0;
 bool closed = false, can = true; std::string wire; std::function<void()> onAdd; std::function<void()> disconnected;
 std::size_t space() { return window; }
 std::size_t add(const char *p, std::size_t n) {
  assert(n <= ASYNC_RESPONCE_BUFF_SIZE);
  ++addCalls;
  if (onAdd) onAdd();
  const std::size_t got = std::min(n, std::min(window, limit));
  wire.append(p, got);
  window -= got;
  return got;
 }
 bool send() { ++sends; return !closed; }
 void close() { ++closes; closed = true; auto cb = disconnected; disconnected = {}; if (cb) cb(); }
 void abort() { ++aborts; closed = true; auto cb = disconnected; disconnected = {}; if (cb) cb(); }
 bool canSend() { return can && !closed; }
 void setRxTimeout(unsigned) {}
 void onDisconnect(std::nullptr_t) { disconnected = {}; }
};

class AsyncWebServerRequest;
using AsyncWebServerRequestPtr=std::weak_ptr<AsyncWebServerRequest>;
class AsyncWebServerResponse {
protected:
 int _code = 200; String _contentType; std::size_t _contentLength = 0;
 bool _sendContentLength = true, _chunked = false;
 std::size_t _sentLength = 0, _ackedLength = 0, _writtenLength = 0;
 WebResponseState _state = RESPONSE_CONTENT;
public:
 virtual ~AsyncWebServerResponse() {}
 virtual bool _sourceValid() const { return false; }
 virtual bool _finished() const;
 virtual void _respond(AsyncWebServerRequest *) = 0;
 virtual std::size_t _ack(AsyncWebServerRequest *, std::size_t, std::uint32_t) = 0;
 virtual int status() const { return _code; }
 WebResponseState state() const { return _state; }
 std::size_t announced() const { return _contentLength; }
};
#include "Abstract.inc"

class AsyncWebServerRequest {
public:
 Client *_client;
 std::function<void()> _onDisconnectfn;
 AsyncWebServerResponse *_response = nullptr;
 bool _sent = false, _paused = false;
 std::shared_ptr<AsyncWebServerRequest> _this;
 void *_tempObject = nullptr;
 fs::File _tempFile;
 uint8_t *_itemBuffer = nullptr;
#include "Fields.inc"
 AsyncWebServerRequest(Client *c) : _client(c) { if (c) c->disconnected = [this] { _onDisconnect(); }; }
 static std::shared_ptr<AsyncWebServerRequest> create(Client *c) {
  AsyncWebServerRequest *req = new (std::nothrow) AsyncWebServerRequest(c);
  if (req) { req->_this = std::shared_ptr<AsyncWebServerRequest>(req); return req->_this; }
  return {};
 }
#include "API.inc"
 ~AsyncWebServerRequest();
 void _onPoll(); void _onDisconnect(); void _send(); void abort(); AsyncWebServerRequestPtr pause();
 void send(AsyncWebServerResponse *); void onDisconnect(std::function<void()>);
 void send(int code);
 void send(int code, const char *, const char *);
 void _onAck(std::size_t, std::uint32_t); void _onTimeout(std::uint32_t);
 std::shared_ptr<AsyncWebServerRequest> shared_from_this() { return _this; }
 Client *client() const { return _client; }
 bool isSent() const { return _sent; }
 bool isPaused() const { return _paused; }
 AsyncWebServerResponse *getResponse() const { return _response; }
};
struct PlainResponse : AsyncWebServerResponse {
 explicit PlainResponse(int code) { _code = code; }
 void _respond(AsyncWebServerRequest *) override { _state = RESPONSE_END; }
 std::size_t _ack(AsyncWebServerRequest *, std::size_t, std::uint32_t) override { return 0; }
 bool _sourceValid() const override { return true; }
};
inline void AsyncWebServerRequest::send(int code) { send(new PlainResponse(code)); }
inline void AsyncWebServerRequest::send(int code, const char *, const char *) { send(code); }
#include "Methods.inc"
// Only the no-template dispatch branch is modeled; the template processor is not run.
inline size_t AsyncAbstractResponse::_fillBufferAndProcessTemplates(uint8_t *b, size_t n) {
 assert(!_callback);
 return _fillBuffer(b, n);
}
// Header bytes are a four-byte marker, not an HTTP header serialization test.
inline void AsyncAbstractResponse::_respond(AsyncWebServerRequest *r) {
 _assembled_headers = "HEAD";
 _state = RESPONSE_HEADERS;
 write_send_buffs(r, 0, 0);
}
#include "Transport208.inc"
#include "ResponseTail208.inc"
