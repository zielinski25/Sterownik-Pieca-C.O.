#include "UploadPump.h"
#include "Fixture201.inc"
using upload202::Admission;using upload202::Step;
template<size_t F=8,size_t B=32>struct Combined:Fixture {
 Lock queueMutex;using Queue=upload202::Pump<Registry,2,F,B>;
 history197::Permanent<Queue>qs;Queue&q;Combined():q(qs.construct(r,queueMutex)){}
};
static void*volatile roots[512]{};static unsigned used=0;
template<size_t F=8,size_t B=32>static Combined<F,B>&fresh(){assert(used<512);auto*p=new Combined<F,B>;roots[used++]=p;return *p;}
template<class Q>static void drain(Q&q){for(unsigned n=0;n<1000;++n){auto s=q.runOne();assert(s!=Step::Busy);if(s==Step::Idle)return;}assert(false);}
int main(){unsigned groups=0;
 for(size_t n:{size_t(0),size_t(1),size_t(31),size_t(32),size_t(33),size_t(255),size_t(256)}){
  auto&f=fresh();std::string source(n,'x');char name[]="new";auto t=f.q.attach(1,n,true);
  assert(f.q.enqueue(t,name,3,0,reinterpret_cast<const unsigned char*>(source.data()),n,true)==Admission::Accepted);assert(f.q.end(t));assert(f.raw.calls==0&&!f.q.takeReply(t).emit);
  source.assign(n,'z');std::memcpy(name,"bad",3);drain(f.q);auto reply=f.q.takeReply(t);assert(reply.emit&&reply.status==200&&reply.verified==n&&f.model.files["/new"]==std::string(n,'x')&&!f.model.files.count("/bad"));assert(!f.q.takeReply(t).emit);f.q.disconnect(t);drain(f.q);++groups;
 }
 // Production template sizes: full 4096-byte callback leaves room for end/receipt.
 {auto&f=fresh<16,256>();std::string data(4096,'q');auto t=f.q.attach(1,4096,true);assert(f.q.enqueue(t,"big",3,0,reinterpret_cast<const unsigned char*>(data.data()),data.size(),true)==Admission::Accepted&&f.q.end(t));drain(f.q);assert(f.q.takeReply(t).status==200&&f.model.files["/big"]==data);std::cout<<"host sizeof default pump="<<sizeof(f.q)<<" bytes, NOT ESP ABI\n";++groups;}
 // Full queue of one request cannot consume the other request's completion slot.
 {auto&f=fresh();std::string data(256,'a');unsigned char b='b';auto a=f.q.attach(1,256,true),other=f.q.attach(2,1,true);assert(f.q.enqueue(a,"one",3,0,reinterpret_cast<const unsigned char*>(data.data()),256,true)==Admission::Accepted);assert(f.q.enqueue(other,"two",3,0,&b,1,true)==Admission::Rejected);assert(f.q.end(a)&&f.q.end(other));drain(f.q);assert(f.q.takeReply(a).status==200&&f.q.takeReply(other).status==503&&!f.model.files.count("/two"));++groups;}
 // Reservation for ALL pieces, not partial enqueue; failure purges only this request.
 {auto&f=fresh();std::string data(200,'a');auto t=f.q.attach(1,300,true);assert(f.q.enqueue(t,"new",3,0,reinterpret_cast<const unsigned char*>(data.data()),200,false)==Admission::Accepted);assert(f.q.enqueue(t,"new",3,200,reinterpret_cast<const unsigned char*>(data.data()),100,true)==Admission::Rejected);assert(f.q.end(t));drain(f.q);assert(f.q.takeReply(t).status==503&&f.raw.calls==0&&!f.model.files.count("/new"));++groups;}
 // Repeated attach cannot clear a queue failure even after registry cleanup.
 {auto&f=fresh();unsigned char b=1;auto t=f.q.attach(1,300,true);assert(f.q.enqueue(t,"new",3,0,&b,300,true)==Admission::Rejected);drain(f.q);assert(!f.q.takeReply(t).emit);assert(!f.q.attach(1,3,true).generation);auto other=f.q.attach(2,0,true);assert(other.generation);assert(f.q.end(t)&&f.q.takeReply(t).status==503);assert(!f.q.attach(3,0,true).generation);++groups;}
 // Input bounds checked before buffer copy, including overflow.
 for(int bad=0;bad<4;++bad){auto&f=fresh();unsigned char b=1;auto t=f.q.attach(1,3,true);auto result=f.q.enqueue(t,bad==0?nullptr:"new",bad==1?64:3,bad==2?size_t(-1):0,bad==3?nullptr:&b,3,true);assert(result==Admission::Rejected&&f.raw.calls==0);f.q.end(t);assert(f.q.takeReply(t).status==400);++groups;}
 // Missing file and denied authorization remain errors after asynchronous processing.
 for(bool allowed:{false,true}){auto&f=fresh();auto t=f.q.attach(1,0,allowed);if(!allowed)assert(f.q.enqueue(t,"new",3,0,nullptr,0,true)==Admission::Accepted);f.q.end(t);drain(f.q);assert(f.q.takeReply(t).status==(allowed?400:403)&&f.raw.calls==0);++groups;}
 // Disconnect before work: no open. Stale token cannot touch a reused request.
 {auto&f=fresh();unsigned char b=1;auto old=f.q.attach(1,1,true);f.q.enqueue(old,"old",3,0,&b,1,true);assert(f.q.disconnect(old));drain(f.q);assert(f.raw.calls==0);auto t=f.q.attach(1,1,true);assert(t.generation!=old.generation);assert(f.q.enqueue(old,"bad",3,0,&b,1,true)==Admission::Stale&&!f.q.disconnect(old));f.q.enqueue(t,"new",3,0,&b,1,true);f.q.end(t);drain(f.q);assert(f.q.takeReply(t).status==200);++groups;}
 // Disconnect after one owned frame: queued tail discarded, partial file retained.
 {auto&f=fresh();std::string data(100,'a');auto t=f.q.attach(1,100,true);f.q.enqueue(t,"new",3,0,reinterpret_cast<const unsigned char*>(data.data()),100,true);f.q.end(t);assert(f.q.runOne()==Step::Worked);auto calls=f.raw.calls;assert(f.q.disconnect(t)&&f.raw.calls==calls);drain(f.q);assert(f.model.files["/new"]==std::string(32,'a')&&!f.q.takeReply(t).emit);++groups;}
 // Queued response invalidated by disconnect; no send or raw pointer is retained.
 {auto&f=fresh();unsigned char b=1;auto t=f.q.attach(1,1,true);f.q.enqueue(t,"new",3,0,&b,1,true);f.q.end(t);drain(f.q);assert(f.q.disconnect(t)&&!f.q.takeReply(t).emit);drain(f.q);++groups;}
 // Callback after end cannot silently append data to an already ended request.
 {auto&f=fresh();unsigned char b=1;auto t=f.q.attach(1,1,true);f.q.end(t);assert(f.q.enqueue(t,"new",3,0,&b,1,true)==Admission::Rejected);assert(f.q.takeReply(t).status==400&&f.raw.calls==0);++groups;}
 // Every backend fault still suppresses success through the queue and registry.
 {auto&base=fresh();unsigned char b=1;auto t=base.q.attach(1,1,true);base.q.enqueue(t,"new",3,0,&b,1,true);base.q.end(t);drain(base.q);assert(base.q.takeReply(t).status==200);auto count=base.raw.calls;unsigned injected=0;
  for(size_t at=1;at<=count;++at)for(auto mode:{Raw::Negative,Raw::PositiveError,Raw::Stop}){auto&f=fresh();f.raw.target=at;f.raw.mode=mode;auto token=f.q.attach(1,1,true);f.q.enqueue(token,"new",3,0,&b,1,true);f.q.end(token);drain(f.q);auto reply=f.q.takeReply(token);assert(reply.emit&&reply.status>=500&&f.raw.calls==at&&!f.o.ready());f.q.disconnect(token);drain(f.q);assert(f.raw.calls==at);++injected;}
  std::cout<<"queue syscall-fault cases="<<injected<<"\n";++groups;
 }
 // Worker in flight: independent ingress, single-consumer guard, disconnect race.
 {auto&f=fresh();unsigned char b=7;auto t=f.q.attach(1,1,true);f.q.enqueue(t,"one",3,0,&b,1,true);f.q.end(t);std::mutex m;std::condition_variable cv;bool entered=false,release=false;
  f.model.hook=[&](const char*op){if(std::strcmp(op,"fd.write"))return;std::unique_lock<std::mutex>l(m);entered=true;cv.notify_all();cv.wait(l,[&]{return release;});};
  std::thread worker([&]{assert(f.q.runOne()==Step::Worked);});{std::unique_lock<std::mutex>l(m);cv.wait(l,[&]{return entered;});}
  assert(f.q.runOne()==Step::Busy);auto other=f.q.attach(2,1,true);assert(other.generation&&f.q.enqueue(other,"two",3,0,&b,1,true)==Admission::Accepted);f.q.end(other);b=99;auto calls=f.raw.calls;assert(f.q.disconnect(t)&&f.raw.calls==calls);
  {std::lock_guard<std::mutex>l(m);release=true;cv.notify_all();}worker.join();f.model.hook={};drain(f.q);assert(!f.q.takeReply(t).emit&&f.q.takeReply(other).status==200&&f.model.files["/two"]==std::string(1,char(7))&&f.model.files["/one"]==std::string(1,char(7)));++groups;
 }
 std::cout<<"PASS queue202 groups="<<groups<<"; owned buffers + real 197/200/201/FS.cpp; not AsyncTCP/ESP\n";
}
