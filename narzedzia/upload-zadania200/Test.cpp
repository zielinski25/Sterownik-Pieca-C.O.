#include "UploadPool.h"
#include "Model197.inc"
#include "Raw198.inc"
#include <chrono>
template<size_t Slots=2,class Epoch=uint64_t>struct Fixture {
 Model model;Lock lock;Backend backend;history197::Permanent<Owner>os;Owner&o;Raw raw;
 using Pool=upload200::Pool<Owner,Slots,4096,Epoch>;history197::Permanent<Pool>ps;Pool&p;
 Fixture():backend(model),o(os.construct(backend,lock)),raw(model,o),p(ps.construct(o,raw)){assert(o.enable());}
};
static void*roots[1024]{};static unsigned used=0;
template<size_t Slots=2,class Epoch=uint64_t>static Fixture<Slots,Epoch>&fresh(){assert(used<1024);auto*p=new Fixture<Slots,Epoch>;roots[used++]=p;return *p;}
static const unsigned char abc[]={'a',0,255};
static void waitFor(const std::function<bool()>&condition){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!condition()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
int main(){unsigned groups=0,faults=0;
 for(size_t n:{size_t(0),size_t(1),size_t(255),size_t(256),size_t(257),size_t(4096)}){
  auto&f=fresh();std::string bytes(n,'x');auto t=f.p.begin("new.bin",7,n);assert(f.p.chunk(t,0,reinterpret_cast<const unsigned char*>(bytes.data()),n,true));auto r=f.p.complete(t);assert(r.emit&&r.status==200&&r.verified==n&&f.model.files["/new.bin"]==bytes&&f.raw.fds.empty());assert(std::count(f.raw.trace.begin(),f.raw.trace.end(),"sync")==static_cast<long>((n+255)/256+1));assert(!f.p.complete(t).emit);assert(f.p.release(t));assert(!f.p.complete(t).emit);++groups;
 }
 // File final is NOT whole-request completion: no premature sync/close/success.
 {auto&f=fresh();auto t=f.p.begin("binary",6,3);assert(f.p.chunk(t,0,abc,3,true));assert(f.raw.fds.size()==1&&std::count(f.raw.trace.begin(),f.raw.trace.end(),"close")==0);auto r=f.p.complete(t);assert(r.status==200&&r.verified==3&&f.model.files["/binary"]==std::string(reinterpret_cast<const char*>(abc),3));++groups;}
 // Fault every real call position: negative, success+errno, or stop in operation.
 {auto&base=fresh();std::string data(601,'x');auto t=base.p.begin("new",3,data.size());assert(base.p.chunk(t,0,reinterpret_cast<const unsigned char*>(data.data()),data.size(),true));assert(base.p.complete(t).status==200);auto count=base.raw.calls;
  for(size_t at=1;at<=count;++at)for(auto mode:{Raw::Negative,Raw::PositiveError,Raw::Stop}){
   auto&f=fresh();auto file=f.o.fs().open("/a");f.raw.target=at;f.raw.mode=mode;auto ticket=f.p.begin("new",3,data.size());f.p.chunk(ticket,0,reinterpret_cast<const unsigned char*>(data.data()),data.size(),true);auto r=f.p.complete(ticket);
   assert(r.emit&&r.status!=200&&!f.o.ready()&&f.o.drained()&&f.raw.calls==at);auto calls=f.raw.calls,events=f.model.events.size();
   assert(!f.p.chunk(ticket,0,abc,3,true)&&!f.p.complete(ticket).emit);f.p.release(ticket);assert(f.raw.calls==calls);assert(!file&&!f.o.fs().open("/a","w"));file.close();assert(f.model.events.size()==events&&f.model.closes==0);
   assert(f.p.retained()==f.raw.fds.size()+(f.raw.trace.back()=="close"?1:0));++faults;
  }++groups;
 }
 // Protocol errors: gap/replay/oversize/missing final/too short/no progress/null/second file.
 for(int which=0;which<8;++which){auto&f=fresh();auto t=f.p.begin("new",3,3);
  if(which==0)assert(!f.p.chunk(t,1,abc,3,true));
  if(which==1){assert(f.p.chunk(t,0,abc,1,false));assert(!f.p.chunk(t,0,abc,1,false));}
  if(which==2)assert(!f.p.chunk(t,0,abc,4,true));
  if(which==3)assert(f.p.chunk(t,0,abc,3,false));
  if(which==4)assert(!f.p.chunk(t,0,abc,1,true));
  if(which==5)assert(!f.p.chunk(t,0,abc,0,false));
  if(which==6)assert(!f.p.chunk(t,0,nullptr,3,true));
  if(which==7){assert(f.p.chunk(t,0,abc,3,true));assert(!f.p.chunk(t,0,abc,3,true));}
  auto r=f.p.complete(t);assert(r.emit&&r.status==400&&f.o.ready());assert(std::count(f.raw.trace.begin(),f.raw.trace.end(),"close")==1);assert(f.p.release(t)&&f.raw.fds.empty());assert(f.model.files.count("/new"));++groups;
 }
 // Empty request is not a successful empty file. Empty FILE requires final=true.
 {auto&f=fresh();auto t=f.p.begin("empty",5,0);assert(f.p.complete(t).status==400);++groups;}
 // Validate name and bounds before raw open, including embedded NUL and reserved names.
 for(const char*name:{"","../x","a/b","a\\b","a..b",".hidden","hist_short.csv","hist_long.csv","hist_short.csv.rebuild","hist_long.csv.rebuild","history_reset_a1.req","history_reset_a1.done","log_a.txt","log_b.txt","log_krytyczny.txt","panel_firmware.bin"}){auto&f=fresh();auto t=f.p.begin(name,std::strlen(name),3);assert(f.p.complete(t).status==400&&f.raw.calls==0&&f.o.ready());++groups;}
 {auto&f=fresh();const char name[]={'a',0,'b'};auto t=f.p.begin(name,3,3);assert(f.p.complete(t).status==400&&f.raw.calls==0);++groups;}
 {auto&f=fresh();auto t=f.p.begin("new",3,4097);assert(f.p.complete(t).status==413&&f.raw.calls==0);++groups;}
 // One request cannot clear another's failure. Same filename cannot overwrite first.
 {auto&f=fresh();auto a=f.p.begin("one",3,3),b=f.p.begin("two",3,3);assert(!f.p.chunk(a,2,abc,3,true));assert(f.p.chunk(b,0,abc,3,true));assert(f.p.complete(b).status==200&&f.p.complete(a).status==400&&f.model.files["/two"].size()==3);++groups;}
 {auto&f=fresh();auto a=f.p.begin("same",4,3),b=f.p.begin("same",4,0);assert(f.p.complete(b).status==409&&f.o.ready());assert(f.p.chunk(a,0,abc,3,true)&&f.p.complete(a).status==200);++groups;}
 // Pool exhaustion is checked before opening; stale token cannot hit reused slot.
 {auto&f=fresh<1>();auto a=f.p.begin("one",3,3);auto calls=f.raw.calls;auto unavailable=f.p.begin("two",3,3);assert(unavailable.slot==1&&f.raw.calls==calls&&!f.p.complete(unavailable).emit);assert(f.p.release(a));auto b=f.p.begin("two",3,3);calls=f.raw.calls;assert(!f.p.chunk(a,0,abc,3,true)&&!f.p.release(a)&&!f.p.complete(a).emit&&f.raw.calls==calls);assert(f.p.chunk(b,0,abc,3,true)&&f.p.complete(b).status==200);++groups;}
 // Small epoch type exercises exhaustion without wrap/ABA; slots retire forever.
 {auto&f=fresh<1,uint8_t>();for(unsigned i=0;i<255;++i){std::string name="n"+std::to_string(i);auto t=f.p.begin(name.data(),name.size(),0);assert(t.slot==0&&t.generation==i+1);assert(f.p.chunk(t,0,nullptr,0,true)&&f.p.complete(t).status==200&&f.p.release(t));}auto calls=f.raw.calls;assert(f.p.begin("last",4,0).slot==1&&f.raw.calls==calls);++groups;}
 // Disconnect retains partial file, closes once if healthy; stale completion cannot report OK.
 {auto&f=fresh();auto t=f.p.begin("part",4,3);assert(f.p.chunk(t,0,abc,1,false));assert(f.p.release(t)&&f.model.files["/part"].size()==1);assert(!f.p.complete(t).emit);++groups;}
 // A failed cancel close is not retried, even if backend did not consume FD.
 {auto&f=fresh();auto t=f.p.begin("part",4,3);f.raw.target=f.raw.calls+1;f.raw.mode=Raw::Negative;f.raw.consumeClose=false;assert(!f.p.release(t)&&!f.o.ready());auto n=f.raw.calls;assert(!f.p.release(t)&&f.raw.calls==n&&f.p.retained()==1&&f.p.complete(t).status==500);++groups;}
 // Data corruption and incorrect final size are terminal, not just short-write checks.
 for(bool corrupt:{false,true}){auto&f=fresh();auto t=f.p.begin("new",3,3);f.raw.corrupt=corrupt;f.raw.skewSize=!corrupt;f.p.chunk(t,0,abc,3,true);assert(f.p.complete(t).status==500&&!f.o.ready()&&f.p.retained()==1);++groups;}
 // Expected ENODATA only on stat; normal finished transaction remains healthy.
 {auto&f=fresh();f.raw.enodata=true;auto t=f.p.begin("new",3,3);assert(f.p.chunk(t,0,abc,3,true)&&f.p.complete(t).status==200);++groups;}
 // Stop after final chunk but before whole request: no close/success on uncertain storage.
 {auto&f=fresh();auto t=f.p.begin("new",3,3);assert(f.p.chunk(t,0,abc,3,true));auto calls=f.raw.calls;f.o.trip();assert(f.p.complete(t).status==503&&f.raw.calls==calls&&!f.p.release(t));++groups;}
 // Success already returned is not retroactively changed by a later stop.
 {auto&f=fresh();auto t=f.p.begin("new",3,3);assert(f.p.chunk(t,0,abc,3,true));auto r=f.p.complete(t);f.o.trip();assert(r.status==200&&!f.p.complete(t).emit&&f.p.release(t));++groups;}
 // Two real threads use different descriptors under the same Owner lock.
 {auto&f=fresh();auto a=f.p.begin("one",3,50),b=f.p.begin("two",3,50);auto run=[&](typename Fixture<>::Pool::Ticket t,unsigned char c){for(size_t i=0;i<50;++i)assert(f.p.chunk(t,i,&c,1,i==49));assert(f.p.complete(t).status==200);};std::thread x(run,a,'x'),y(run,b,'y');x.join();y.join();assert(f.model.files["/one"]==std::string(50,'x')&&f.model.files["/two"]==std::string(50,'y'));++groups;}
 // Signal stop with write in flight and a second upload waiting on the owner.
 {auto&f=fresh();auto a=f.p.begin("one",3,3),b=f.p.begin("two",3,3);std::mutex m;std::condition_variable cv;bool entered=false,release=false;bool first=true,second=true;
  f.model.hook=[&](const char*op){if(std::strcmp(op,"fd.write"))return;std::unique_lock<std::mutex>lock(m);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});};
  std::thread x([&]{first=f.p.chunk(a,0,abc,3,true);});{std::unique_lock<std::mutex>lock(m);cv.wait(lock,[&]{return entered;});}
  std::thread y([&]{second=f.p.chunk(b,0,abc,3,true);});waitFor([&]{return f.lock.waiting.load()!=0;});f.o.trip();{std::lock_guard<std::mutex>lock(m);release=true;cv.notify_all();}x.join();y.join();f.model.hook={};assert(!first&&!second&&f.o.drained()&&f.p.complete(a).status!=200&&f.p.complete(b).status!=200);assert(std::count(f.raw.trace.begin(),f.raw.trace.end(),"write")==1&&std::count(f.raw.trace.begin(),f.raw.trace.end(),"close")==0);++groups;
 }
 // Denial must also apply to a disconnect arriving directly after stop.
 {auto&f=fresh();auto t=f.p.begin("new",3,3);auto calls=f.raw.calls;f.o.trip();assert(!f.p.release(t)&&f.raw.calls==calls&&f.p.retained()==1);++groups;}
 // Overflow-sized metadata is rejected before touching the caller's buffer.
 for(bool offset:{false,true}){auto&f=fresh();auto t=f.p.begin("new",3,3);assert(!f.p.chunk(t,offset?size_t(-1):0,abc,offset?3:size_t(-1),true));assert(f.p.complete(t).status==400&&f.o.ready()&&std::count(f.raw.trace.begin(),f.raw.trace.end(),"write")==0);++groups;}
 for(const char*op:{"read","write"})for(auto mode:{Raw::ShortCount,Raw::LargeCount}){auto&base=fresh();auto t=base.p.begin("new",3,3);assert(base.p.chunk(t,0,abc,3,true));size_t at=std::find(base.raw.trace.begin(),base.raw.trace.end(),op)-base.raw.trace.begin()+1;auto&f=fresh();f.raw.target=at;f.raw.mode=mode;t=f.p.begin("new",3,3);assert(!f.p.chunk(t,0,abc,3,true));assert(f.p.complete(t).status==500&&!f.o.ready()&&f.raw.calls==at);++groups;}
 std::cout<<"PASS upload200 groups="<<groups<<" syscall-faults="<<faults<<"; per-request state, checked sync/readback/close, real FS.cpp; not HTTP/ESP\n";
}
