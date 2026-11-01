#pragma once
#include "UploadPool.h"
namespace upload201 {
enum class Action { Accepted, Rejected, Stale, Busy, Emit, Silent };
struct Result {Action action;int status;std::size_t bytes;};
// key and token are SERVER INTERNAL, not credentials or client-supplied IDs.
// chunk/complete/service run in an FS-qualified worker with owned input buffers.
// disconnect performs bookkeeping only, and may run while that worker is busy.
template<class Pool,std::size_t Capacity=2,class Epoch=uint64_t>class Registry {
 static_assert(Capacity>0&&std::is_unsigned<Epoch>::value,"bounded unsigned registry");
 struct Slot {
  bool occupied=false,busy=false,disconnected=false,retained=false,seen=false,hasTicket=false,replied=false;
  Epoch generation=0;uintptr_t key=0;std::size_t expected=0,nameLength=0;char name[64]{};int error=0;
  typename Pool::Ticket ticket{};
 } slots_[Capacity];
 Pool&pool_;history197::Mutex&mutex_;
 struct Guard {history197::Mutex&m;explicit Guard(history197::Mutex&x):m(x){m.lock();}~Guard(){m.unlock();}};
public:
 struct Token {std::size_t slot;Epoch generation;};
 Registry(Pool&p,history197::Mutex&m):pool_(p),mutex_(m){}
 ~Registry()=delete;Registry(const Registry&)=delete;Registry&operator=(const Registry&)=delete;
private:
 Slot*find(Token t){if(t.slot>=Capacity)return nullptr;auto&s=slots_[t.slot];return s.occupied&&s.generation==t.generation?&s:nullptr;}
 Action take(Token t,Slot&work){Guard g(mutex_);auto*s=find(t);if(!s||s->disconnected)return Action::Stale;if(s->busy)return Action::Busy;s->busy=true;work=*s;return Action::Accepted;}
 Result publish(Token t,const Slot&work,Result result){Guard g(mutex_);auto*s=find(t);if(!s)return {Action::Stale,0,0};bool gone=s->disconnected;*s=work;s->disconnected=gone;s->busy=false;return gone?Result{Action::Silent,0,0}:result;}
 void cancel(Slot&w){if(w.hasTicket){if(pool_.release(w.ticket))w.hasTicket=false;else{auto r=pool_.complete(w.ticket);if(r.status>=500)w.error=r.status;}}}
public:
 // Invalid token: caller retains its own admission error, never defaults to 200.
 Token attach(uintptr_t key,std::size_t expected,bool authorized){Guard g(mutex_);
  if(!key)return {Capacity,0};
  for(const auto&s:slots_)if(s.occupied&&s.key==key)return {Capacity,0};
  for(std::size_t i=0;i<Capacity;++i){auto&s=slots_[i];if(!s.occupied&&s.generation<std::numeric_limits<Epoch>::max()){
   Epoch next=s.generation+1;s=Slot{};s.occupied=true;s.generation=next;s.key=key;s.expected=expected;s.error=authorized?0:403;return {i,next};
  }}return {Capacity,0};
 }
 Result chunk(Token t,const char*name,std::size_t nameLength,std::size_t index,const unsigned char*data,std::size_t len,bool final){
  Slot w;Action a=take(t,w);if(a!=Action::Accepted)return {a,0,0};
  if(w.replied)return publish(t,w,{Action::Silent,0,0});
  Result r{Action::Rejected,w.error,0};
  if(!w.error&&!w.replied){
   if(!name||!nameLength||nameLength>63||(w.seen&&(index==0||nameLength!=w.nameLength||std::memcmp(name,w.name,nameLength)))||(!w.seen&&index!=0))w.error=400;
   else {
    if(!w.seen){w.seen=true;w.nameLength=nameLength;std::memcpy(w.name,name,nameLength);w.ticket=pool_.begin(name,nameLength,w.expected);w.hasTicket=w.ticket.generation!=0;if(!w.hasTicket)w.error=503;}
    if(!w.error){if(pool_.chunk(w.ticket,index,data,len,final))r={Action::Accepted,0,0};else{auto reply=pool_.complete(w.ticket);w.error=reply.status?reply.status:400;}}
   }
   if(w.error)cancel(w);
  }
  if(w.error)r={Action::Rejected,w.error,0};
  return publish(t,w,r);
 }
 Result complete(Token t){
  Slot w;Action a=take(t,w);if(a!=Action::Accepted)return {a,0,0};
  Result r{Action::Silent,0,0};
  if(!w.replied){
   if(!w.seen&&!w.error)w.error=400;
   if(w.error){cancel(w);r={Action::Emit,w.error,0};}
   else{auto reply=pool_.complete(w.ticket);r={reply.emit?Action::Emit:Action::Silent,reply.status,reply.verified};}
   w.replied=true;
  }return publish(t,w,r);
 }
 bool disconnect(Token t){Guard g(mutex_);auto*s=find(t);if(!s)return false;s->disconnected=true;return true;}
 // At most one cleanup per call. No automatic retry of a quarantined release.
 bool service(){
  Slot w;Token t{Capacity,0};
  {Guard g(mutex_);for(std::size_t i=0;i<Capacity;++i){auto&s=slots_[i];if(s.occupied&&s.disconnected&&!s.busy&&!s.retained){s.busy=true;w=s;t={i,s.generation};break;}}}
  if(t.slot==Capacity)return false;
  const bool released=!w.hasTicket||pool_.release(w.ticket);
  {Guard g(mutex_);auto*s=find(t);s->busy=false;if(released)s->occupied=false;else s->retained=true;}
  return true;
 }
};
}
