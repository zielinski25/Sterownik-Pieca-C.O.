#include "Fixture203.inc"
#include "UploadAdapter.h"
struct Harness:Combined<> {
 Stats stats;AsyncWebServer server{&stats};Services services{stats};
 using Adapter=upload204::Adapter<Queue,Services>;using Code=Adapter::Code;
 history197::Permanent<Adapter>storage;Adapter&a;
 Harness():a(storage.construct(q,services)){}
 std::shared_ptr<AsyncWebServerRequest>request(){return AsyncWebServerRequest::create(&server,new AsyncClient(stats));}
 Adapter::Handle begin(std::shared_ptr<AsyncWebServerRequest>r,size_t expected=1){auto b=a.begin(*r,expected,true,100);assert(b.code==Code::Accepted);assert(!r->isPaused());return b.handle;}
 void chunk(Adapter::Handle h,std::shared_ptr<AsyncWebServerRequest>r,const char*name="new"){unsigned char b=42;assert(a.chunk(h,*r,name,strlen(name),0,&b,1,true)==Admission::Accepted);}
};
using Code=Harness::Code;
static Harness*volatile roots[256]{};static unsigned used=0;
static Harness&fresh(){assert(used<256);auto*f=new Harness;roots[used++]=f;return *f;}
template<class Q>static void drain(Q&q){for(unsigned i=0;i<1000;++i){auto step=q.runOne();assert(step!=Step::Busy);if(step==Step::Idle)return;}assert(false);}
static void finish(std::shared_ptr<AsyncWebServerRequest>&r){auto weak=AsyncWebServerRequestPtr(r);r->client()->close();r.reset();assert(weak.expired());}
int main(){unsigned groups=0;
 // Whole request end is independent of file-final; worker cannot send.
 {auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);drain(f.q);r->_onPoll();assert(!r->isPaused()&&!r->isSent()&&!f.stats.responds);assert(f.a.end(h,*r)&&r->isPaused());std::thread w([&]{drain(f.q);});w.join();assert(!f.stats.responds);auto io=f.raw.calls;r->_onPoll();assert(f.stats.status==200&&f.raw.calls==io&&f.stats.responds==1);r->_onPoll();assert(f.stats.responds==1);finish(r);drain(f.q);++groups;}
 // Fixed capacity before worker work; duplicate cannot consume another slot.
 {auto&f=fresh();auto a=f.request(),b=f.request(),c=f.request();auto ha=f.begin(a);assert(f.a.begin(*a,1,true,100).code==Code::Conflict);auto hb=f.begin(b);assert(f.a.begin(*c,1,true,100).code==Code::Full);f.chunk(ha,a,"one");f.chunk(hb,b,"two");assert(f.a.end(ha,*a)&&f.a.end(hb,*b));drain(f.q);a->_onPoll();b->_onPoll();assert(f.stats.responds==2);finish(a);finish(b);finish(c);drain(f.q);++groups;}
 // Disconnect does not release adapter storage while old request still exists.
 {auto&f=fresh();auto a=f.request(),b=f.request(),c=f.request();f.begin(a);f.begin(b);a->client()->close();drain(f.q);assert(f.a.begin(*c,1,true,100).code==Code::Full);a.reset();auto h=f.begin(c);f.chunk(h,c);assert(f.a.end(h,*c));drain(f.q);c->_onPoll();assert(f.stats.status==200);finish(b);finish(c);drain(f.q);++groups;}
 // Same slot, new generation: delayed callbacks cannot change new request.
 {auto&f=fresh();auto r=f.request();auto old=f.begin(r);finish(r);drain(f.q);r=f.request();auto h=f.begin(r);assert(h.slot==old.slot&&h.generation!=old.generation);assert(f.a.chunk(old,*r,"bad",3,0,nullptr,0,true)==Admission::Stale);assert(!f.a.end(old,*r));f.chunk(h,r);assert(f.a.end(h,*r));drain(f.q);r->_onPoll();assert(f.stats.status==200);finish(r);drain(f.q);++groups;}
 // Different request cannot use another live handle, even with matching generation.
 {auto&f=fresh();auto a=f.request(),b=f.request();auto ha=f.begin(a),hb=f.begin(b);assert(f.a.chunk(ha,*b,"bad",3,0,nullptr,0,true)==Admission::Stale);assert(!f.a.end(ha,*b));f.chunk(hb,b);assert(f.a.end(hb,*b));drain(f.q);b->_onPoll();assert(f.stats.status==200);finish(a);finish(b);drain(f.q);++groups;}
 // Disconnect during reception cancels queued writes without performing I/O.
 {auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);finish(r);assert(f.raw.calls==0);drain(f.q);assert(f.raw.calls==0);++groups;}
 // Hook conflict after attach must cancel the accepted token (repeated failures).
 {auto&f=fresh();for(int i=0;i<8;++i){auto r=f.request();assert(r->installDeferred203([](AsyncWebServerRequest*,void*){},[](void*){},nullptr));assert(f.a.begin(*r,1,true,100).code==Code::Conflict);finish(r);drain(f.q);}auto r=f.request();auto h=f.begin(r);f.chunk(h,r);assert(f.a.end(h,*r));drain(f.q);r->_onPoll();assert(f.stats.status==200);finish(r);drain(f.q);++groups;}
 // Unauthorized and invalid timeout requests consume no capacity/storage.
 {auto&f=fresh();for(unsigned i=0;i<8;++i){auto r=f.request();assert(f.a.begin(*r,1,false,100).code==Code::Forbidden);assert(f.a.begin(*r,1,true,0).code==Code::Invalid);assert(f.a.begin(*r,1,true,0x80000000u).code==Code::Invalid);finish(r);}assert(f.raw.calls==0);auto r=f.request();f.begin(r);finish(r);drain(f.q);++groups;}
 // Already paused/sent requests remain owned by their original handler.
 {auto&f=fresh();auto r=f.request();r->pause();assert(f.a.begin(*r,1,true,100).code==Code::Invalid);r->send(new AsyncWebServerResponse(f.stats,409));assert(f.a.begin(*r,1,true,100).code==Code::Invalid);finish(r);++groups;}
 // Duplicate end and late chunks never enqueue more file data.
 {auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);assert(f.a.end(h,*r));assert(!f.a.end(h,*r));assert(f.a.chunk(h,*r,"bad",3,1,nullptr,0,true)==Admission::Stale);drain(f.q);r->_onPoll();assert(f.stats.status==200);finish(r);drain(f.q);++groups;}
 // Deadline from begin includes reception and waiting, despite closed send window.
 for(bool end:{false,true}){auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);if(end)assert(f.a.end(h,*r));f.stats.canSend=false;f.stats.now=99;r->_onPoll();assert(!f.stats.aborts);f.stats.now=100;r->_onPoll();assert(f.stats.aborts==1&&f.raw.calls==0);r.reset();drain(f.q);assert(f.raw.calls==0);++groups;}
 {auto&f=fresh();f.stats.now=0xfffffff0U;auto r=f.request();auto b=f.a.begin(*r,1,true,32);assert(b.code==Code::Accepted);f.stats.now=15;r->_onPoll();assert(!f.stats.aborts);f.stats.now=16;r->_onPoll();assert(f.stats.aborts==1);r.reset();drain(f.q);++groups;}
 // Queue overload produces terminal 503, not truncated successful storage.
 {auto&f=fresh();auto r=f.request();auto h=f.begin(r,300);unsigned char data[300]{};assert(f.a.chunk(h,*r,"new",3,0,data,300,true)==Admission::Rejected);assert(f.a.end(h,*r));drain(f.q);r->_onPoll();assert(f.stats.status==503&&f.raw.calls==0);finish(r);drain(f.q);++groups;}
 // Incomplete file and storage error do not become success.
 for(bool io:{false,true}){auto&f=fresh();auto r=f.request();auto h=f.begin(r);if(io){f.raw.target=1;f.raw.mode=Raw::Negative;f.chunk(h,r);}assert(f.a.end(h,*r));drain(f.q);r->_onPoll();assert(f.stats.status!=200&&f.stats.responds==1);finish(r);drain(f.q);++groups;}
 // Response factory races; no leaks, no replacement, no duplicate send on reentry.
 for(unsigned mode=0;mode<4;++mode){auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);assert(f.a.end(h,*r));drain(f.q);auto*raw=r.get();if(mode==0)f.stats.onMake=[raw]{raw->abort();};if(mode==1)f.stats.oom=true;if(mode==2)f.stats.onMake=[&]{raw->send(new AsyncWebServerResponse(f.stats,409));};if(mode==3)f.stats.onMake=[raw]{raw->_onPoll();};r->_onPoll();f.stats.onMake={};if(mode<2){assert(f.stats.responds==0&&f.stats.aborts==1);r.reset();}else{assert(f.stats.responds==1&&f.stats.status==(mode==2?409:200));finish(r);}drain(f.q);assert(f.stats.responsesMade==f.stats.responsesDestroyed);++groups;}
 // Strong request lifetime pin even when the only reference is internal.
 {auto&f=fresh();auto r=f.request();f.begin(r);auto*raw=r.get();auto weak=AsyncWebServerRequestPtr(r);r.reset();f.stats.now=100;raw->_onPoll();assert(weak.expired());drain(f.q);++groups;}
 // User disconnect handler can reenter admission but cannot reuse its own live slot.
 {auto&f=fresh();auto a=f.request(),b=f.request(),c=f.request();f.begin(a);f.begin(b);unsigned called=0;a->onDisconnect([&]{++called;assert(f.a.begin(*c,1,true,100).code==Code::Full);});finish(a);assert(called==1);drain(f.q);f.begin(c);finish(b);finish(c);drain(f.q);++groups;}
 // Worker in flight at peer disconnect: no network API in worker and no I/O in callback.
 {auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);std::mutex m;std::condition_variable cv;bool entered=false,release=false;f.model.hook=[&](const char*op){if(strcmp(op,"fd.write"))return;std::unique_lock<std::mutex>l(m);entered=true;cv.notify_all();cv.wait(l,[&]{return release;});};std::thread w([&]{f.q.runOne();});{std::unique_lock<std::mutex>l(m);cv.wait(l,[&]{return entered;});}auto io=f.raw.calls;finish(r);assert(f.raw.calls==io);{std::lock_guard<std::mutex>l(m);release=true;cv.notify_all();}w.join();f.model.hook={};drain(f.q);assert(!f.stats.responds);++groups;}
 // Fifty sequential connections use the same bounded adapter, not new contexts.
 {auto&f=fresh();uint64_t previous=0;for(unsigned i=0;i<50;++i){auto r=f.request();auto h=f.begin(r);assert(h.slot==0&&h.generation>previous);previous=h.generation;auto name="f"+std::to_string(i);f.chunk(h,r,name.c_str());assert(f.a.end(h,*r));drain(f.q);r->_onPoll();assert(f.stats.status==200);finish(r);drain(f.q);}assert(f.stats.responds==50&&f.stats.responsesMade==f.stats.responsesDestroyed);++groups;}
 // Full underlying queue is distinct from full adapter; do not steal its token.
 {auto&f=fresh();auto a=f.q.attach(123,1,true),b=f.q.attach(456,1,true);assert(a.generation&&b.generation);auto r=f.request();assert(f.a.begin(*r,1,true,100).code==Code::Unavailable);f.q.disconnect(a);f.q.disconnect(b);drain(f.q);f.begin(r);finish(r);drain(f.q);++groups;}
 // Invalid handles cannot address a slot or finish a live request.
 {auto&f=fresh();auto r=f.request();auto h=f.begin(r);for(Harness::Adapter::Handle bad: {Harness::Adapter::Handle{2,1},Harness::Adapter::Handle{h.slot,0}}){assert(!f.a.end(bad,*r));assert(f.a.chunk(bad,*r,"bad",3,0,nullptr,0,true)==Admission::Stale);}finish(r);drain(f.q);++groups;}
 // Ready result stays queued until writable; no response allocations while blocked.
 {auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);assert(f.a.end(h,*r));drain(f.q);f.stats.canSend=false;r->_onPoll();assert(!f.stats.responsesMade);f.stats.canSend=true;r->_onPoll();assert(f.stats.status==200);finish(r);drain(f.q);++groups;}
 // Foreign handler response before end or poll cancels accepted storage work.
 for(bool end: {false,true}){auto&f=fresh();auto r=f.request();auto h=f.begin(r);f.chunk(h,r);if(end)assert(f.a.end(h,*r));r->send(new AsyncWebServerResponse(f.stats,409));if(!end){assert(!f.a.end(h,*r));}else{r->_onPoll();}drain(f.q);assert(f.raw.calls==0);finish(r);drain(f.q);++groups;}
 std::cout<<"PASS adapter204 groups="<<groups<<"; bounded begin/chunk/end; modeled TCP/parser, not ESP; host sizeof="<<sizeof(Harness::Adapter)<<"\n";
}
