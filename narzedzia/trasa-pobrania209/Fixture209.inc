// Stage 209 fixture: real request parsing (206), real route selection from the pinned
// library (209), real 199 send loop and 203 request hooks, real 207 pin, real 208 adapter.
// Modeled: the TCP client, the four header bytes, error bodies and the String shim.
#include "Model197.inc"
#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <list>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#ifndef ASYNCWEBSERVER_USE_CHUNK_INFLIGHT
#define ASYNCWEBSERVER_USE_CHUNK_INFLIGHT 0
#endif
#define CONFIG_LWIP_TCP_MSS 32
#define CONFIG_LWIP_TCP_WND_DEFAULT 64
#define ASYNC_RESPONCE_BUFF_SIZE (CONFIG_LWIP_TCP_MSS * 2)
#define RESPONSE_TRY_AGAIN 0xFFFFFFFF
#define RESPONSE_STREAM_BUFFER_SIZE 1460
#define async_ws_log_v(...) ((void)0)
#define async_ws_log_d(...) ((void)0)
#define async_ws_log_e(...) ((void)0)
static inline bool isParamChar(char c) { return c && c != '{' && c != '[' && c != '&' && c != '='; }
using AwsTemplateProcessor = std::function<String(const String &)>;
using ArDisconnectHandler = std::function<void()>;

#include "Arduino.h"
#include "literals.h"
using namespace asyncsrv;
#include "Enums206.inc"

enum WebResponseState { RESPONSE_SETUP, RESPONSE_HEADERS, RESPONSE_CONTENT, RESPONSE_WAIT_ACK, RESPONSE_END, RESPONSE_FAILED };

class AsyncWebServerRequest;
using AsyncWebServerRequestPtr = std::weak_ptr<AsyncWebServerRequest>;

// Modeled TCP client: keeps a receive window and a per-write acceptance limit so the
// real send loop sees partial writes exactly like a slow peer.
struct Client {
 std::size_t window = 4096, limit = 4096, addCalls = 0, sends = 0, closes = 0, aborts = 0;
 bool closed = false, can = true;
 std::string wire;
 std::function<void()> disconnected;
 std::size_t space() { return window; }
 std::size_t write(const char *p, std::size_t n) { wire.append(p, n); return n; }
 std::size_t add(const char *p, std::size_t n) {
  assert(n <= ASYNC_RESPONCE_BUFF_SIZE);
  ++addCalls;
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

using AsyncClient = Client;

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
};
#include "Abstract.inc"

class AsyncWebHeader {
 String n_, v_;
public:
 AsyncWebHeader() {}
 AsyncWebHeader(const String &n, const String &v) : n_(n), v_(v) {}
 const String &name() const { return n_; }
 const String &value() const { return v_; }
 operator bool() const { return n_.length() != 0; }
 static const AsyncWebHeader parse(const String &d) { return parse(d.c_str()); }
 static const AsyncWebHeader parse(const char *);
};
struct AsyncWebParameter {
 String n, v;
 bool post, file;
 std::size_t size;
 AsyncWebParameter(String a, String b, bool p = false, bool f = false, std::size_t z = 0) : n(a), v(b), post(p), file(f), size(z) {}
 bool isPost() const { return post; }
 bool isFile() const { return file; }
 const String &name() const { return n; }
};

class AsyncWebHandler {
protected:
 bool (*_filter)(AsyncWebServerRequest *) = nullptr;
public:
 virtual ~AsyncWebHandler() {}
 virtual bool canHandle(AsyncWebServerRequest *) const = 0;
 virtual void handleRequest(AsyncWebServerRequest *) = 0;
 virtual void handleUpload(AsyncWebServerRequest *, const String &, std::size_t, uint8_t *, std::size_t, bool) = 0;
 virtual void handleBody(AsyncWebServerRequest *, uint8_t *, std::size_t, std::size_t, std::size_t) {}
 virtual bool isRequestHandlerTrivial() { return false; }
 virtual bool mustSkipServerMiddlewares() { return false; }
 void _runChain(AsyncWebServerRequest *, std::function<void()>) {}
 // Pinned inline body, checked by build.py: return _filter == NULL || _filter(request);
 bool filter(AsyncWebServerRequest *request) const { return _filter == NULL || _filter(request); }
};

#include "Real209Decl.inc"

class AsyncCallbackWebHandler : public AsyncWebHandler {
protected:
 AsyncURIMatcher _uri;
 WebRequestMethodComposite _method;
 std::function<void(AsyncWebServerRequest *)> _onRequest;
 std::function<void(AsyncWebServerRequest *, const String &, std::size_t, uint8_t *, std::size_t, bool)> _onUpload;
public:
 AsyncCallbackWebHandler() : _uri(), _method(WebRequestMethodComposite(AsyncWebRequestMethod::HTTP_INVALID) | AsyncWebRequestMethod::HTTP_GET) {}
 void setUri(AsyncURIMatcher uri) { _uri = std::move(uri); }
 void setMethod(WebRequestMethodComposite method) { _method = method; }
 void onRequest(std::function<void(AsyncWebServerRequest *)> fn) { _onRequest = fn; }
 bool canHandle(AsyncWebServerRequest *request) const override;
 void handleRequest(AsyncWebServerRequest *request) override;
 void handleUpload(AsyncWebServerRequest *, const String &, std::size_t, uint8_t *, std::size_t, bool) override {}
};

struct AsyncWebServer {
 typedef bool (*AttachPolicy206)(AsyncWebServerRequest *, void *);
 std::vector<std::unique_ptr<AsyncWebHandler>> _handlers;
 AsyncCallbackWebHandler *_catchAllHandler = nullptr;
 AttachPolicy206 _attachPolicy206 = nullptr;
 void *_attachPolicyContext206 = nullptr;
 void addHandler(AsyncWebHandler *h) { _handlers.emplace_back(h); }
 void _rewriteRequest(AsyncWebServerRequest *) {}
 void _attachHandler(AsyncWebServerRequest *);
 bool setAttachPolicy206(AttachPolicy206 policy, void *context);
 void clearAttachPolicy206();
 bool _runAttachPolicy206(AsyncWebServerRequest *request);
};

class AsyncWebServerRequest {
public:
 AsyncWebServer *_server;
 Client *_client;
 AsyncWebHandler *_handler = nullptr;
 std::function<void()> _onDisconnectfn;
 AsyncWebServerResponse *_response = nullptr;
 bool _sent = false, _paused = false;
 std::shared_ptr<AsyncWebServerRequest> _this;
 void *_tempObject = nullptr;
 fs::File _tempFile;
 uint8_t *_itemBuffer = nullptr;
 // Real parsing fields, same names and types as the pinned request.
 String _temp, _contentType, _boundary, _host, _url, _authorization;
 String _itemName, _itemFilename, _itemType, _itemValue;
 AsyncAuthType _authMethod = AUTH_NONE;
 RequestedConnectionType _reqconntype = RCT_HTTP;
 std::list<AsyncWebHeader> _headers;
 std::list<AsyncWebParameter> _params;
 WebRequestMethod _method = AsyncWebRequestMethod::HTTP_UNKNOWN;
 uint8_t _version = 0, _parseState = PARSE_REQ_START, _multiParseState = 0, _chunkedParseState = CHUNK_NONE, _chunkedLastChar = 0;
 bool _isMultipart = false, _isPlainPost = false, _expectingContinue = false, _itemIsFile = false;
 std::size_t _chunkOffset = 0, _chunkSize = 0, _chunkStartIndex = 0;
 std::size_t _contentLength = 0, _parsedLength = 0, _boundaryPosition = 0, _itemStartIndex = 0, _itemSize = 0, _itemBufferIndex = 0;
#include "Fields.inc"
#include "Fields205.inc"
 AsyncWebServerRequest(AsyncWebServer *server, Client *client) : _server(server), _client(client) {
  if (client) client->disconnected = [this] { _onDisconnect(); };
 }
#include "Factory.inc"
#include "API.inc"
#include "API205.inc"
 ~AsyncWebServerRequest();
 void _onPoll(); void _onDisconnect(); void _send(); void abort(); AsyncWebServerRequestPtr pause();
 void send(AsyncWebServerResponse *); void onDisconnect(std::function<void()>);
 void send(int code); void send(int code, const char *, const char *);
 void _onAck(std::size_t, std::uint32_t); void _onTimeout(std::uint32_t);
 std::shared_ptr<AsyncWebServerRequest> shared_from_this() { return _this; }
 Client *client() const { return _client; }
 bool isSent() const { return _sent; }
 bool isPaused() const { return _paused; }
 AsyncWebServerResponse *getResponse() const { return _response; }
 void setHandler(AsyncWebHandler *h) { _handler = h; }
 uint8_t version() const { return _version; }
 WebRequestMethod method() const { return _method; }
 const String &url() const { return _url; }
 const String &host() const { return _host; }
 const String &contentType() const { return _contentType; }
 std::size_t contentLength() const { return _contentLength; }
 bool multipart() const { return _isMultipart; }
 AsyncAuthType authType() const { return _authMethod; }
 const String &authChallenge() const { return _authorization; }
 bool isHTTP() const { return _reqconntype == RCT_HTTP; }
 std::size_t headers() const; const AsyncWebHeader *getHeader(const char *) const; const AsyncWebHeader *getHeader(std::size_t) const;
 const String &header(const char *) const; const String &header(std::size_t) const; const String &headerName(std::size_t) const;
 bool _parseReqHead(); bool _parseReqHeader(); String urlDecode(const String &) const; void _addGetParams(const String &);
 void _parseLine(); void _onData(void *, std::size_t); void _handleUploadByte(uint8_t, bool); void _parseMultipartPostByte(uint8_t, bool);
 void _parsePlainPostChar(uint8_t); bool _parseChunkedBytes(uint8_t *, std::size_t); void _runMiddlewareChain();
};

// Error responses carry the status code only; the body is empty on purpose.
struct ErrorResponse final : AsyncAbstractResponse {
 explicit ErrorResponse(int code) : AsyncAbstractResponse(nullptr) {
  _code = code;
  _contentType = T_text_plain;
  _contentLength = 0;
  _sendContentLength = true;
  _chunked = false;
 }
 bool _sourceValid() const override { return true; }
 std::size_t _fillBuffer(uint8_t *, std::size_t) override { return 0; }
};
inline void AsyncWebServerRequest::send(int code) { send(new ErrorResponse(code)); }
inline void AsyncWebServerRequest::send(int code, const char *, const char *) { send(code); }
#include "Methods.inc"
inline void AsyncWebServerRequest::_runMiddlewareChain() { if (_handler) _handler->handleRequest(this); }
inline bool AsyncWebServerRequest::_parseChunkedBytes(uint8_t *, std::size_t) { assert(false); return false; }
inline void AsyncWebServerRequest::_parsePlainPostChar(uint8_t) {}
inline size_t AsyncAbstractResponse::_fillBufferAndProcessTemplates(uint8_t *b, size_t n) {
 assert(!_callback);
 return _fillBuffer(b, n);
}
inline void AsyncAbstractResponse::_respond(AsyncWebServerRequest *r) {
 _assembled_headers = "HEAD";
 _state = RESPONSE_HEADERS;
 write_send_buffs(r, 0, 0);
}
#include "Transport208.inc"
#include "ResponseTail208.inc"
namespace asyncsrv { WebRequestMethod stringToMethod(const String &); }
#include "AsyncWebHeader.inc"
#include "Methods206.inc"
#include "Parser206.inc"
#include "Real209Impl.inc"
