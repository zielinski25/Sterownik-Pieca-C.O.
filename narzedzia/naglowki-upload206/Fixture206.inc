#include "Fixture202.inc"
#define T_text_plain "text/plain"
#include <memory>
#include <functional>
#include <vector>
#include <list>
#include <string>
#include <thread>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#define async_ws_log_v(...) ((void)0)
#define async_ws_log_d(...) ((void)0)
#define async_ws_log_e(...) ((void)0)
#define RESPONSE_STREAM_BUFFER_SIZE 1460
using ArDisconnectHandler=std::function<void()>;
struct Stats {
 std::thread::id network=std::this_thread::get_id();uint32_t now=0;bool canSend=true,oom=false,sourceValid=true;
 unsigned aborts=0,closes=0,clientsDestroyed=0,responsesMade=0,responsesDestroyed=0,responds=0,acks=0,timeouts=0;int status=0;
 std::function<void()> onMake,onRespond;
 void check(){assert(std::this_thread::get_id()==network);}
};
static inline bool isParamChar(char c){return c&&c!='{'&&c!='['&&c!='&'&&c!='=';}
struct AsyncWebParameter {
 String n,v;bool post,file;size_t size;
 AsyncWebParameter(String a,String b,bool p=false,bool f=false,size_t z=0):n(a),v(b),post(p),file(f),size(z){}
 bool isPost()const{return post;}bool isFile()const{return file;}const String&name()const{return n;}
};
class AsyncWebServerRequest;
using AsyncWebServerRequestPtr=std::weak_ptr<AsyncWebServerRequest>;
class AsyncWebHeader {
 String n_,v_;
public:
 AsyncWebHeader(){}AsyncWebHeader(const String&n,const String&v):n_(n),v_(v){}
 const String&name()const{return n_;}const String&value()const{return v_;}
 operator bool()const{return n_.length();}
 static const AsyncWebHeader parse(const String&d){return parse(d.c_str());}
 static const AsyncWebHeader parse(const char*);
};
struct AsyncWebServerResponse {
 Stats&s;int code;bool valid;
 AsyncWebServerResponse(Stats&x,int c):s(x),code(c),valid(x.sourceValid){++s.responsesMade;}
 virtual ~AsyncWebServerResponse(){s.check();++s.responsesDestroyed;}
 virtual bool _sourceValid()const{return valid;}
 virtual void _respond(AsyncWebServerRequest*){s.check();++s.responds;s.status=code;if(s.onRespond)s.onRespond();}
 virtual size_t _ack(AsyncWebServerRequest*,size_t,uint32_t){s.check();++s.acks;return 0;}
};
struct AsyncClient {
 Stats&s;std::function<void()> disconnected;std::string written;
 explicit AsyncClient(Stats&x):s(x){}~AsyncClient(){s.check();++s.clientsDestroyed;}
 void onDisconnect(std::nullptr_t){disconnected={};}
 void abort(){s.check();++s.aborts;auto cb=disconnected;if(cb)cb();}
 void close(){s.check();++s.closes;auto cb=disconnected;if(cb)cb();}
 bool canSend(){s.check();return s.canSend;}
 void setRxTimeout(unsigned){s.check();++s.timeouts;}
 size_t write(const char*p,size_t n){s.check();written.append(p,n);return n;}
};
class AsyncWebHandler {
public:
 virtual ~AsyncWebHandler(){}
 virtual bool filter(AsyncWebServerRequest*)const{return true;}
 virtual bool canHandle(AsyncWebServerRequest*)const=0;
 virtual void handleRequest(AsyncWebServerRequest*)=0;
 virtual void handleUpload(AsyncWebServerRequest*,const String&,size_t,uint8_t*,size_t,bool)=0;
 virtual void handleBody(AsyncWebServerRequest*,uint8_t*,size_t,size_t,size_t){assert(false);}
 virtual bool isRequestHandlerTrivial(){return false;}
 virtual bool mustSkipServerMiddlewares(){return false;}
 void _runChain(AsyncWebServerRequest*,std::function<void()>){}
};
namespace asyncsrv {WebRequestMethod stringToMethod(const String&);}
struct AsyncWebServer {
 Stats*stats;explicit AsyncWebServer(Stats*x):stats(x){}
 std::vector<AsyncWebHandler*>handlers;AsyncWebHandler*catchAll=nullptr;
 typedef bool(*AttachPolicy206)(AsyncWebServerRequest*,void*);
 bool setAttachPolicy206(AttachPolicy206,void*);
 void clearAttachPolicy206();
 bool _runAttachPolicy206(AsyncWebServerRequest*);
 AttachPolicy206 _attachPolicy206=nullptr;void*_attachPolicyContext206=nullptr;
 void _attachHandler(AsyncWebServerRequest*);
 void addHandler(AsyncWebHandler*h){handlers.push_back(h);}
 void _rewriteRequest(AsyncWebServerRequest*){}
};
class AsyncWebServerRequest {
public:
 Stats&s;AsyncClient*_client;AsyncWebServer*_server;AsyncWebHandler*_handler=nullptr;
 AsyncWebServerResponse*_response=nullptr;std::function<void()>_onDisconnectfn;
 bool _sent=false,_paused=false;std::shared_ptr<AsyncWebServerRequest>_this;
 void*_tempObject=nullptr;fs::File _tempFile;uint8_t*_itemBuffer=nullptr;
 String _temp,_contentType,_boundary,_host,_url,_authorization;
 String _itemName,_itemFilename,_itemType,_itemValue;
 AsyncAuthType _authMethod=AUTH_NONE;
 RequestedConnectionType _reqconntype=RCT_HTTP;
 std::list<AsyncWebHeader> _headers;
 std::list<AsyncWebParameter> _params;
 WebRequestMethod _method=AsyncWebRequestMethod::HTTP_UNKNOWN;
 uint8_t _version=0,_parseState=PARSE_REQ_START,_multiParseState=0,_chunkedParseState=CHUNK_NONE,_chunkedLastChar=0;
 bool _isMultipart=false,_isPlainPost=false,_expectingContinue=false,_itemIsFile=false;
 size_t _chunkOffset=0,_chunkSize=0,_chunkStartIndex=0;
 size_t _contentLength=0,_parsedLength=0,_boundaryPosition=0,_itemStartIndex=0,_itemSize=0,_itemBufferIndex=0;
#include "Fields.inc"
#include "Fields205.inc"
 AsyncWebServerRequest(AsyncWebServer*server,AsyncClient*client):s(*server->stats),_client(client),_server(server){client->disconnected=[this]{_onDisconnect();};}
#include "Factory.inc"
#include "API.inc"
#include "API205.inc"
 ~AsyncWebServerRequest();void _onPoll();void _onDisconnect();void _send();void abort();AsyncWebServerRequestPtr pause();
 void send(AsyncWebServerResponse*);void onDisconnect(std::function<void()>);
 void send(int code,const char*,const char*){send(new AsyncWebServerResponse(s,code));}
 std::shared_ptr<AsyncWebServerRequest>shared_from_this(){return _this;}
 AsyncClient*client()const{return _client;}bool isSent()const{return _sent;}bool isPaused()const{return _paused;}
 AsyncWebServerResponse*getResponse()const{return _response;}
 void setHandler(AsyncWebHandler*h){_handler=h;}
 uint8_t version()const{return _version;}
 WebRequestMethod method()const{return _method;}
 const String&url()const{return _url;}
 const String&host()const{return _host;}
 const String&contentType()const{return _contentType;}
 size_t contentLength()const{return _contentLength;}
 bool multipart()const{return _isMultipart;}
 AsyncAuthType authType()const{return _authMethod;}
 const String&authChallenge()const{return _authorization;}
 bool isHTTP()const{return _reqconntype==RCT_HTTP;}
 size_t headers()const;const AsyncWebHeader*getHeader(const char*)const;const AsyncWebHeader*getHeader(size_t)const;
 const String&header(const char*)const;const String&header(size_t)const;const String&headerName(size_t)const;
 bool _parseReqHead();bool _parseReqHeader();String urlDecode(const String&)const;void _addGetParams(const String&);
 void _parseLine();void _onData(void*,size_t);void _handleUploadByte(uint8_t,bool);void _parseMultipartPostByte(uint8_t,bool);
 void _parsePlainPostChar(uint8_t);bool _parseChunkedBytes(uint8_t*,size_t);void _runMiddlewareChain();
};
#include "Methods.inc"
inline void AsyncWebServer::_attachHandler(AsyncWebServerRequest*r){
 for(auto*h:handlers)if(h->filter(r)&&h->canHandle(r)){r->setHandler(h);return;}
 r->setHandler(catchAll);
}
inline void AsyncWebServerRequest::_runMiddlewareChain(){if(_handler)_handler->handleRequest(this);}
inline bool AsyncWebServerRequest::_parseChunkedBytes(uint8_t*,size_t){assert(false);return false;}
inline void AsyncWebServerRequest::_parsePlainPostChar(uint8_t){assert(false);}
#include "AsyncWebHeader.inc"
#include "Methods206.inc"
#include "Parser206.inc"
struct Services {
 Stats&s;explicit Services(Stats&x):s(x){}
 uint32_t now(){s.check();return s.now;}
 AsyncWebServerResponse*makeResponse(int code){s.check();if(s.onMake)s.onMake();return s.oom?nullptr:new AsyncWebServerResponse(s,code);}
};
