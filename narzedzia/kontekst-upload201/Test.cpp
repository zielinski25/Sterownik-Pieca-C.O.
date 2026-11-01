#include "RequestRegistry.h"
#include "Model197.inc"
#include "Raw198.inc"
using upload201::Action;
struct Fixture {
 Model model;Lock lock,binding;Backend backend;history197::Permanent<Owner>os;Owner&o;Raw raw;
 using Pool=upload200::Pool<Owner>;history197::Permanent<Pool>ps;Pool&p;
 using Registry=upload201::Registry<Pool>;history197::Permanent<Registry>rs;Registry&r;
 Fixture():backend(model),o(os.construct(backend,lock)),raw(model,o),p(ps.construct(o,raw)),r(rs.construct(p,binding)){assert(o.enable());}
};
static Fixture*roots[256]{};static unsigned used=0;
static Fixture&fresh(){assert(used<256);return *(roots[used++]=new Fixture);}
static const unsigned char bytes[]={'a','b','c'};
int main(){unsigned groups=0;
 {auto&f=fresh();auto t=f.r.attach(1,3,true);assert(f.r.chunk(t,"new",3,0,bytes,3,true).action==Action::Accepted);auto out=f.r.complete(t);assert(out.action==Action::Emit&&out.status==200&&out.bytes==3);assert(f.r.complete(t).action==Action::Silent);auto calls=f.raw.calls;assert(f.r.chunk(t,"new",3,0,bytes,3,true).action==Action::Silent&&f.raw.calls==calls);assert(f.r.disconnect(t)&&f.r.service());assert(f.r.chunk(t,"new",3,0,bytes,3,true).action==Action::Stale);++groups;}
 for(int scenario=0;scenario<6;++scenario){auto&f=fresh();auto t=f.r.attach(1,3,scenario!=0);
  if(scenario==0)assert(f.r.chunk(t,"new",3,0,bytes,3,true).status==403);
  if(scenario==2)f.r.chunk(t,"new",3,1,bytes,3,true);
  if(scenario==3){assert(f.r.chunk(t,"new",3,0,bytes,1,false).action==Action::Accepted);f.r.chunk(t,"bad",3,1,bytes+1,2,true);}
  if(scenario==4){assert(f.r.chunk(t,"new",3,0,bytes,3,true).action==Action::Accepted);f.r.chunk(t,"next",4,0,bytes,3,true);}
  if(scenario==5)f.r.chunk(t,"new",3,0,bytes,3,false);
  auto out=f.r.complete(t);assert(out.action==Action::Emit&&out.status==(scenario==0?403:400));assert(f.r.complete(t).action==Action::Silent);if(scenario<=2)assert(f.raw.calls==0);assert(!f.model.files.count("/next"));f.r.disconnect(t);assert(f.r.service());++groups;
 }
 // Duplicate attach cannot reset current failure. Two requests stay separate.
 {auto&f=fresh();auto a=f.r.attach(1,3,true);assert(f.r.attach(1,3,true).generation==0);auto b=f.r.attach(2,3,true);f.r.chunk(a,"one",3,2,bytes,3,true);assert(f.r.chunk(b,"two",3,0,bytes,3,true).action==Action::Accepted);assert(f.r.complete(a).status==400&&f.r.complete(b).status==200);assert(f.r.attach(3,3,true).generation==0);++groups;}
 // Same address/key reused after cleanup, stale generation still rejected.
 {auto&f=fresh();auto a=f.r.attach(1,3,true);f.r.disconnect(a);assert(f.r.service());auto b=f.r.attach(1,3,true);assert(a.generation!=b.generation);assert(!f.r.disconnect(a));assert(f.r.complete(a).action==Action::Stale);assert(f.r.chunk(b,"new",3,0,bytes,3,true).action==Action::Accepted&&f.r.complete(b).status==200);++groups;}
 // Pool200 capacity refusal is not converted into success or mistaken for file data.
 {auto&f=fresh();auto p=f.p.begin("one",3,0),q=f.p.begin("two",3,0);(void)p;(void)q;auto t=f.r.attach(1,3,true);assert(f.r.chunk(t,"new",3,0,bytes,3,true).status==503);assert(f.r.complete(t).status==503&&!f.model.files.count("/new"));++groups;}
 // Disconnect itself does zero filesystem work. Cleanup is explicitly serviced.
 {auto&f=fresh();auto t=f.r.attach(1,3,true);f.r.chunk(t,"new",3,0,bytes,1,false);auto n=f.raw.calls;assert(f.r.disconnect(t)&&f.raw.calls==n);assert(f.r.complete(t).action==Action::Stale);assert(f.r.service()&&f.raw.calls==n+1&&!f.r.service());assert(f.model.files["/new"]=="a");++groups;}
 // Failed cleanup consumes no further backend operations on later service polls.
 {auto&f=fresh();auto t=f.r.attach(1,3,true);f.r.chunk(t,"new",3,0,bytes,1,false);f.r.disconnect(t);f.raw.target=f.raw.calls+1;f.raw.mode=Raw::Negative;assert(f.r.service());auto n=f.raw.calls;for(int i=0;i<20;++i)assert(!f.r.service());assert(f.raw.calls==n&&!f.o.ready()&&f.p.retained()==1&&f.r.attach(1,3,true).generation==0);++groups;}
 // Error on protocol cleanup takes precedence over the protocol's 400.
 {auto&f=fresh();auto t=f.r.attach(1,3,true);f.r.chunk(t,"new",3,0,bytes,1,false);f.raw.target=f.raw.calls+1;f.raw.mode=Raw::Negative;assert(f.r.chunk(t,"next",4,0,bytes,3,true).status==500);assert(f.r.complete(t).status==500);++groups;}
 // A storage callback can disconnect without recursive lock/deadlock.
 {auto&f=fresh();auto t=f.r.attach(1,3,true);f.model.hook=[&](const char*op){if(!std::strcmp(op,"fd.write"))assert(f.r.disconnect(t));};auto result=f.r.chunk(t,"new",3,0,bytes,3,true);f.model.hook={};assert(result.action==Action::Silent&&f.r.service());assert(f.r.complete(t).action==Action::Stale);++groups;}
 // Real race: worker has FS in flight, disconnect and service can run concurrently.
 {auto&f=fresh();auto t=f.r.attach(1,3,true);std::mutex m;std::condition_variable cv;bool entered=false,release=false;upload201::Result out{};
 f.model.hook=[&](const char*op){if(std::strcmp(op,"fd.write"))return;std::unique_lock<std::mutex>l(m);entered=true;cv.notify_all();cv.wait(l,[&]{return release;});};
 std::thread worker([&]{out=f.r.chunk(t,"new",3,0,bytes,3,true);});{std::unique_lock<std::mutex>l(m);cv.wait(l,[&]{return entered;});}
 assert(f.r.complete(t).action==Action::Busy);assert(f.r.chunk(t,"new",3,0,bytes,3,true).action==Action::Busy);auto n=f.raw.calls;assert(f.r.disconnect(t)&&!f.r.service()&&f.raw.calls==n);assert(f.r.attach(1,3,true).generation==0);
 {std::lock_guard<std::mutex>l(m);release=true;cv.notify_all();}worker.join();f.model.hook={};assert(out.action==Action::Silent&&f.r.service());++groups;}
 // Disconnect during checked close: suppress response, preserve already stored file.
 {auto&f=fresh();auto t=f.r.attach(1,3,true);f.r.chunk(t,"new",3,0,bytes,3,true);f.model.hook=[&](const char*op){if(!std::strcmp(op,"fd.close"))f.r.disconnect(t);};auto out=f.r.complete(t);f.model.hook={};assert(out.action==Action::Silent&&f.model.files["/new"]=="abc"&&f.r.service());++groups;}
 // Every Pool200 syscall error remains non-success through the binding layer.
 {auto&base=fresh();auto t=base.r.attach(1,3,true);base.r.chunk(t,"new",3,0,bytes,3,true);assert(base.r.complete(t).status==200);auto count=base.raw.calls;unsigned cases=0;
  for(size_t at=1;at<=count;++at)for(auto mode:{Raw::Negative,Raw::PositiveError,Raw::Stop}){auto&f=fresh();f.raw.target=at;f.raw.mode=mode;auto token=f.r.attach(1,3,true);f.r.chunk(token,"new",3,0,bytes,3,true);auto out=f.r.complete(token);assert(out.action==Action::Emit&&out.status>=500&&f.raw.calls==at);f.r.disconnect(token);f.r.service();assert(f.raw.calls==at&&!f.r.service());++cases;}
  std::cout<<"injected registry cases="<<cases<<"\n";++groups;
 }
 {struct TinyFixture:Fixture{using Tiny=upload201::Registry<Pool,1,uint8_t>;history197::Permanent<Tiny>storage;Tiny&tiny;TinyFixture():tiny(storage.construct(p,binding)){}};auto*f=new TinyFixture;roots[used++]=f;
  for(unsigned i=1;i<=255;++i){auto t=f->tiny.attach(i,0,false);assert(t.generation==i&&f->tiny.complete(t).status==403);assert(f->tiny.disconnect(t)&&f->tiny.service());}assert(f->tiny.attach(999,0,true).generation==0&&f->raw.calls==0);++groups;
 }
 std::cout<<"PASS registry201 groups="<<groups<<"; real Pool200/Owner197/FS.cpp, modeled request identity, no HTTP/ESP\n";
}
