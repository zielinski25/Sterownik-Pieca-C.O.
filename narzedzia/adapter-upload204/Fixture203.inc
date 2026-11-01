#include "Fixture202.inc"
#include <memory>
#include <functional>
#define async_ws_log_v(...) ((void)0)
#define T_text_plain "text/plain"
using ArDisconnectHandler=std::function<void()>;
struct Stats {
 std::thread::id network=std::this_thread::get_id();uint32_t now=0;bool canSend=true,oom=false,sourceValid=true;
 unsigned aborts=0,closes=0,clientsDestroyed=0,responsesMade=0,responsesDestroyed=0,responds=0,acks=0,timeouts=0;int status=0;
 std::function<void()> onMake,onRespond;
 void check(){assert(std::this_thread::get_id()==network);}
};
class AsyncWebServerRequest;
using AsyncWebServerRequestPtr=std::weak_ptr<AsyncWebServerRequest>;
struct AsyncWebServer {Stats*stats;};
struct AsyncClient {
 Stats&s;std::function<void()> disconnected;
 explicit AsyncClient(Stats&x):s(x){}~AsyncClient(){s.check();++s.clientsDestroyed;}
 void onDisconnect(std::nullptr_t){disconnected={};}
 void abort(){s.check();++s.aborts;auto callback=disconnected;if(callback)callback();}
 void close(){s.check();++s.closes;auto callback=disconnected;if(callback)callback();}
 bool canSend(){s.check();return s.canSend;}
 void setRxTimeout(unsigned){s.check();++s.timeouts;}
};
struct AsyncWebServerResponse {
 Stats&s;int code;bool valid;
 AsyncWebServerResponse(Stats&x,int c):s(x),code(c),valid(x.sourceValid){++s.responsesMade;}
 virtual ~AsyncWebServerResponse(){s.check();++s.responsesDestroyed;}
 virtual bool _sourceValid()const{return valid;}
 virtual void _respond(AsyncWebServerRequest*){s.check();++s.responds;s.status=code;if(s.onRespond)s.onRespond();}
 virtual size_t _ack(AsyncWebServerRequest*,size_t,uint32_t){s.check();++s.acks;return 0;}
};
class AsyncWebServerRequest {
public:
 Stats&s;AsyncClient*_client;AsyncWebServerResponse*_response=nullptr;std::function<void()>_onDisconnectfn;
 bool _sent=false,_paused=false;std::shared_ptr<AsyncWebServerRequest>_this;
 void*_tempObject=nullptr;fs::File _tempFile;void*_itemBuffer=nullptr;
#include "Fields.inc"
 AsyncWebServerRequest(AsyncWebServer*server,AsyncClient*client):s(*server->stats),_client(client){client->disconnected=[this]{_onDisconnect();};}
#include "Factory.inc"
#include "API.inc"
 ~AsyncWebServerRequest();void _onPoll();void _onDisconnect();void _send();void abort();AsyncWebServerRequestPtr pause();
 void send(AsyncWebServerResponse*);void onDisconnect(std::function<void()>);
 void send(int code,const char*,const char*){send(new AsyncWebServerResponse(s,code));}
 std::shared_ptr<AsyncWebServerRequest>shared_from_this(){return _this;}
 AsyncClient*client(){return _client;}bool isSent()const{return _sent;}bool isPaused()const{return _paused;}
 AsyncWebServerResponse*getResponse()const{return _response;}
};
#include "Methods.inc"
#include "DeferredUploadReply.h"
struct Services {
 Stats&s;explicit Services(Stats&x):s(x){}
 uint32_t now(){s.check();return s.now;}
 AsyncWebServerResponse*makeResponse(int code){s.check();if(s.onMake)s.onMake();return s.oom?nullptr:new AsyncWebServerResponse(s,code);}
};
