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
struct Harness:Combined<> {
 Stats stats;AsyncWebServer server{&stats};Services services{stats};
 using Driver=upload203::DeferredUploadReply<Queue,Services>;history197::Permanent<Driver>ds;Driver*driver=nullptr;Queue::Token token{};
 std::shared_ptr<AsyncWebServerRequest> request(uint32_t timeout=100,bool authorized=true,bool bind=true){
  auto request=AsyncWebServerRequest::create(&server,new AsyncClient(stats));auto key=reinterpret_cast<uintptr_t>(request.get());token=q.attach(key,1,authorized);unsigned char b=42;assert(q.enqueue(token,"new",3,0,&b,1,true)==Admission::Accepted);assert(q.end(token));driver=&ds.construct(q,token,key,services);if(bind)assert(driver->bind(*request,timeout));return request;
 }
};
static Harness*volatile roots[256]{};static unsigned used=0;
static Harness&fresh(){assert(used<256);auto*f=new Harness;roots[used++]=f;return *f;}
template<class Q>static void drain(Q&q){for(unsigned i=0;i<1000;++i){auto step=q.runOne();assert(step!=Step::Busy);if(step==Step::Idle)return;}assert(false);}
static void finish(std::shared_ptr<AsyncWebServerRequest>&r){auto weak=AsyncWebServerRequestPtr(r);r->client()->close();r.reset();assert(weak.expired());}
int main(){unsigned groups=0;
 // Worker completes storage only. Response creation/sending occurs on a network poll.
 {auto&f=fresh();auto r=f.request();r->_onPoll();assert(!r->isSent()&&!f.stats.responds);std::thread worker([&]{drain(f.q);});worker.join();assert(!f.stats.responsesMade&&!r->isSent());auto io=f.raw.calls;r->_onPoll();assert(r->isSent()&&f.stats.status==200&&f.stats.responds==1&&f.raw.calls==io);for(int i=0;i<5;++i)r->_onPoll();assert(f.stats.responds==1);finish(r);drain(f.q);assert(f.stats.responsesMade==f.stats.responsesDestroyed);++groups;}
 // Not writable: retain result; timeout checked before canSend and without a response.
 {auto&f=fresh();auto r=f.request();drain(f.q);f.stats.canSend=false;f.stats.now=99;r->_onPoll();assert(!r->isSent()&&!f.stats.responsesMade);f.stats.now=100;auto io=f.raw.calls;r->_onPoll();assert(r->isSent()&&f.stats.aborts==1&&!f.stats.responsesMade&&f.raw.calls==io);auto w=AsyncWebServerRequestPtr(r);r.reset();assert(w.expired());drain(f.q);++groups;}
 {auto&f=fresh();auto r=f.request();drain(f.q);f.stats.canSend=false;r->_onPoll();f.stats.canSend=true;r->_onPoll();assert(f.stats.status==200&&f.stats.responds==1);finish(r);drain(f.q);++groups;}
 // Timeout while write is still pending: no I/O on poll; worker later cleans up.
 {auto&f=fresh();auto r=f.request();f.stats.now=100;r->_onPoll();assert(f.raw.calls==0&&f.stats.aborts==1);auto w=AsyncWebServerRequestPtr(r);r.reset();assert(w.expired());drain(f.q);assert(f.raw.calls==0);++groups;}
 // Unsigned wrap and boundary; timeout never disabled by a zero configuration.
 {auto&f=fresh();f.stats.now=0xfffffff0U;auto r=f.request(32);f.stats.canSend=false;f.stats.now=15;r->_onPoll();assert(f.stats.aborts==0);f.stats.now=16;r->_onPoll();assert(f.stats.aborts==1);r.reset();drain(f.q);++groups;}
 for(uint32_t timeout:{0u,0x80000000u}){auto&f=fresh();auto r=f.request(100,true,false);assert(!f.driver->bind(*r,timeout)&&!r->_paused);finish(r);f.q.disconnect(f.token);drain(f.q);++groups;}
 // Strong self pin: no external reference; synchronous abort destroys after poll returns.
 {auto&f=fresh();auto r=f.request();auto*raw=r.get();auto weak=AsyncWebServerRequestPtr(r);r.reset();f.stats.now=100;raw->_onPoll();assert(weak.expired()&&f.stats.clientsDestroyed==1);drain(f.q);++groups;}
 // Reentrant poll ignored, not a second delivery; request may disappear in the hook.
 {auto&f=fresh();auto r=f.request();drain(f.q);unsigned factoryCalls=0;auto*raw=r.get();f.stats.onMake=[&]{++factoryCalls;raw->_onPoll();};r->_onPoll();assert(factoryCalls==1&&f.stats.responds==1);f.stats.onMake={};finish(r);drain(f.q);++groups;}
 // Factory disconnect: produced response is destroyed, never sent, no leaked pointer.
 {auto&f=fresh();auto r=f.request();drain(f.q);auto*raw=r.get();auto weak=AsyncWebServerRequestPtr(r);f.stats.onMake=[raw]{raw->abort();};r.reset();raw->_onPoll();f.stats.onMake={};assert(weak.expired()&&f.stats.responds==0&&f.stats.responsesMade==f.stats.responsesDestroyed);drain(f.q);++groups;}
 // Factory OOM signal: abort, never a successful empty reply.
 {auto&f=fresh();auto r=f.request();drain(f.q);f.stats.oom=true;r->_onPoll();assert(f.stats.aborts==1&&!f.stats.responds);r.reset();drain(f.q);++groups;}
 // Another response wins while factory runs; candidate response is deleted, not leaked/replaced.
 {auto&f=fresh();auto r=f.request();drain(f.q);auto*raw=r.get();f.stats.onMake=[&]{raw->send(new AsyncWebServerResponse(f.stats,409));};r->_onPoll();f.stats.onMake={};assert(f.stats.status==409&&f.stats.responds==1&&f.stats.responsesMade==2&&f.stats.responsesDestroyed==1);finish(r);drain(f.q);assert(f.stats.responsesDestroyed==2);++groups;}
 // Existing user disconnect callback preserved, receives shared_from_this, recursion ignored.
 {auto&f=fresh();auto r=f.request();unsigned gone=0;auto*raw=r.get();r->onDisconnect([&]{++gone;assert(raw->shared_from_this());raw->_onDisconnect();});finish(r);assert(gone==1);drain(f.q);++groups;}
 // Abort during response::_respond: real _send still returns through pinned live request.
 {auto&f=fresh();auto r=f.request();drain(f.q);auto*raw=r.get();auto weak=AsyncWebServerRequestPtr(r);f.stats.onRespond=[raw]{raw->abort();};r.reset();raw->_onPoll();f.stats.onRespond={};assert(weak.expired()&&f.stats.acks==0&&f.stats.clientsDestroyed==1);drain(f.q);++groups;}
 // Registration is exclusive; no replacement of existing hook or wrong request binding.
 {auto&f=fresh();auto r=f.request();assert(!f.driver->bind(*r,100));assert(!r->installDeferred203([](AsyncWebServerRequest*,void*){},[](void*){},nullptr));finish(r);drain(f.q);++groups;}
 {auto&f=fresh();auto r=f.request(100,true,false);auto other=AsyncWebServerRequest::create(&f.server,new AsyncClient(f.stats));assert(!f.driver->bind(*other,100));r->send(new AsyncWebServerResponse(f.stats,409));assert(!f.driver->bind(*r,100));finish(other);finish(r);f.q.disconnect(f.token);drain(f.q);++groups;}
 // Permission/storage failures propagate as non-200 through real send/_send methods.
 for(int error=0;error<2;++error){auto&f=fresh();auto r=f.request(100,error!=0);if(error==1){f.raw.target=1;f.raw.mode=Raw::Negative;}drain(f.q);auto io=f.raw.calls;r->_onPoll();assert(f.stats.status==(error==0?403:500)&&f.raw.calls==io);finish(r);drain(f.q);++groups;}
 // Not a download abort fix: pinned abort is a no-op after _sent is already true.
 {auto&f=fresh();auto r=f.request();drain(f.q);r->_onPoll();auto count=f.stats.aborts;r->abort();assert(f.stats.aborts==count&&r->isSent());finish(r);drain(f.q);++groups;}
 // Peer disconnect, not driver's timeout: cancellation still prevents queued storage work.
 {auto&f=fresh();auto r=f.request();finish(r);drain(f.q);assert(f.raw.calls==0);++groups;}
 // Preserve a prior continuation owner: do not bind to an already paused request.
 {auto&f=fresh();auto r=f.request(100,true,false);r->pause();assert(!f.driver->bind(*r,100));finish(r);f.q.disconnect(f.token);drain(f.q);++groups;}
 // Exclusive hook's reentrancy guard is independent of the upload driver's own state.
 {auto&f=fresh();auto r=f.request(100,true,false);unsigned count=0;assert(r->installDeferred203([](AsyncWebServerRequest*req,void*p){auto&n=*static_cast<unsigned*>(p);++n;if(n==1)req->_onPoll();},[](void*){},&count));r->_onPoll();assert(count==1);finish(r);f.q.disconnect(f.token);drain(f.q);++groups;}
 // A foreign response already sent before the poll cancels pending storage work.
 {auto&f=fresh();auto r=f.request();r->send(new AsyncWebServerResponse(f.stats,409));r->_onPoll();drain(f.q);assert(f.stats.status==409&&f.stats.responsesMade==1&&f.raw.calls==0);finish(r);++groups;}
 // Worker in flight while deadline aborts and destroys request: no request pointer in worker.
 {auto&f=fresh();auto r=f.request();auto weak=AsyncWebServerRequestPtr(r);std::mutex m;std::condition_variable cv;bool entered=false,release=false;
  f.model.hook=[&](const char*op){if(std::strcmp(op,"fd.write"))return;std::unique_lock<std::mutex>l(m);entered=true;cv.notify_all();cv.wait(l,[&]{return release;});};
  std::thread worker([&]{assert(f.q.runOne()==Step::Worked);});{std::unique_lock<std::mutex>l(m);cv.wait(l,[&]{return entered;});}
  auto io=f.raw.calls;f.stats.canSend=false;f.stats.now=100;r->_onPoll();r.reset();assert(weak.expired()&&f.stats.clientsDestroyed==1&&f.raw.calls==io);
  {std::lock_guard<std::mutex>l(m);release=true;cv.notify_all();}worker.join();f.model.hook={};drain(f.q);assert(!f.stats.responds&&f.model.files["/new"]==std::string(1,char(42)));++groups;
 }
 // Bounded context reuse: disconnect alone is insufficient while a strong ref exists.
 {auto&f=fresh();auto old=f.request();old->client()->close();drain(f.q);
  auto next=AsyncWebServerRequest::create(&f.server,new AsyncClient(f.stats));auto key=reinterpret_cast<uintptr_t>(next.get());auto token=f.q.attach(key,0,true);assert(token.generation);f.q.enqueue(token,"empty0",6,0,nullptr,0,true);f.q.end(token);
  assert(!f.driver->rearm(token,key));old.reset();assert(f.driver->rearm(token,key)&&f.driver->bind(*next,100));drain(f.q);next->_onPoll();assert(f.stats.status==200);finish(next);drain(f.q);
  for(unsigned i=1;i<=20;++i){auto request=AsyncWebServerRequest::create(&f.server,new AsyncClient(f.stats));key=reinterpret_cast<uintptr_t>(request.get());token=f.q.attach(key,0,true);assert(token.generation);std::string name="empty"+std::to_string(i);f.q.enqueue(token,name.data(),name.size(),0,nullptr,0,true);f.q.end(token);assert(f.driver->rearm(token,key)&&f.driver->bind(*request,100));drain(f.q);request->_onPoll();assert(f.stats.status==200);finish(request);drain(f.q);}
  assert(f.stats.responsesMade==f.stats.responsesDestroyed&&f.stats.clientsDestroyed==22);++groups;
 }
 std::cout<<"PASS deferred203 groups="<<groups<<"; real request factory/pause/send/_send/destructor, patched poll/disconnect; modeled client/response, not ESP\n";
}
