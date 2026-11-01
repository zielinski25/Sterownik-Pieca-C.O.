#pragma once
#include <memory>
#include <limits>
#include <cstdint>
#include <cstddef>
#ifndef HISTORY203_DEFERRED_POLL
#error "Requires complete library patch 203"
#endif
namespace upload204 {
// All adapter calls and hooks: one serialized network context. Worker: Pump only.
// Adapter/Pump/Services have permanent lifetimes, no clientRelease/SSE/WebSocket.
template<class Pump,class Services,std::size_t Capacity=2>class Adapter {
 static_assert(Capacity>0,"nonempty adapter pool");
public:
 struct Handle {std::size_t slot;uint64_t generation;};
 enum class Code { Accepted, Invalid, Forbidden, Full, Unavailable, Conflict };
 struct Begin {Code code;Handle handle;};
private:
 struct Slot {
  Adapter*owner=nullptr;AsyncWebServerRequestPtr request;typename Pump::Token token{};
  uint64_t generation=0;uint32_t started=0,timeout=0;
  bool occupied=false,disconnected=false,waiting=false,done=false;
 } slots_[Capacity];
 Pump&pump_;Services&services_;
 static Handle invalid(){return {Capacity,0};}
 Slot*find(Handle h,AsyncWebServerRequest&r){
  if(h.slot>=Capacity||!h.generation)return nullptr;
  auto&s=slots_[h.slot];
  if(!s.occupied||s.generation!=h.generation||s.disconnected||s.done)return nullptr;
  auto live=s.request.lock();return live&&live.get()==&r?&s:nullptr;
 }
 static void gone(void*p){auto&s=*static_cast<Slot*>(p);s.disconnected=true;s.done=true;s.owner->pump_.disconnect(s.token);}
 static void poll(AsyncWebServerRequest*r,void*p){auto&s=*static_cast<Slot*>(p);s.owner->tick(s,*r);}
 void stop(Slot&s,AsyncWebServerRequest&r){s.done=true;r.stopDeferredPoll203();pump_.disconnect(s.token);}
 void tick(Slot&s,AsyncWebServerRequest&r){
  if(s.done)return;
  if(r.isSent()||r.getResponse()){stop(s,r);return;}
  if(static_cast<uint32_t>(services_.now()-s.started)>=s.timeout){stop(s,r);r.abort();return;}
  if(!s.waiting||!r.client()||!r.client()->canSend())return;
  auto reply=pump_.takeReply(s.token);if(!reply.emit)return;
  std::unique_ptr<AsyncWebServerResponse>response(services_.makeResponse(reply.status));
  if(s.done||r.disconnected203()||r.isSent()||r.getResponse()){stop(s,r);return;}
  if(!response){stop(s,r);r.abort();return;}
  s.done=true;r.stopDeferredPoll203();r.send(response.release());
 }
public:
 Adapter(Pump&p,Services&s):pump_(p),services_(s){}
 ~Adapter()=delete;Adapter(const Adapter&)=delete;Adapter&operator=(const Adapter&)=delete;
 Begin begin(AsyncWebServerRequest&r,std::size_t expected,bool authorized,uint32_t timeout){
  if(!authorized)return {Code::Forbidden,invalid()};
  if(!timeout||timeout>0x7fffffffU||r.isSent()||r.isPaused()||r.getResponse()||r.disconnected203()||!r.client())return {Code::Invalid,invalid()};
  auto keep=r.shared_from_this();if(!keep)return {Code::Invalid,invalid()};
  for(auto&s:slots_)if(s.occupied){auto live=s.request.lock();if(live&&live.get()==&r)return {Code::Conflict,invalid()};}
  std::size_t index=Capacity;
  for(std::size_t i=0;i<Capacity;++i){auto&s=slots_[i];if((!s.occupied||(s.disconnected&&s.request.expired()))&&s.generation!=std::numeric_limits<uint64_t>::max()){index=i;break;}}
  if(index==Capacity)return {Code::Full,invalid()};
  auto token=pump_.attach(reinterpret_cast<uintptr_t>(&r),expected,true);
  if(!token.generation)return {Code::Unavailable,invalid()};
  auto&s=slots_[index];
  // install does not invoke callbacks. On failure, no hook points at this slot.
  if(!r.installDeferred203(poll,gone,&s)){pump_.disconnect(token);return {Code::Conflict,invalid()};}
  ++s.generation;s.owner=this;s.request=keep;s.token=token;s.started=services_.now();s.timeout=timeout;
  s.occupied=true;s.disconnected=false;s.waiting=false;s.done=false;
  return {Code::Accepted,{index,s.generation}};
 }
 upload202::Admission chunk(Handle h,AsyncWebServerRequest&r,const char*name,std::size_t nameLength,std::size_t index,const unsigned char*data,std::size_t length,bool final){
  auto*s=find(h,r);if(!s||s->waiting)return upload202::Admission::Stale;
  return pump_.enqueue(s->token,name,nameLength,index,data,length,final);
 }
 bool end(Handle h,AsyncWebServerRequest&r){
  auto*s=find(h,r);if(!s||s->waiting)return false;
  if(r.isSent()||r.getResponse()||r.isPaused()){stop(*s,r);return false;}
  if(!pump_.end(s->token)){stop(*s,r);r.abort();return false;}
  s->waiting=true;auto weak=r.pause();return !weak.expired()&&!s->disconnected;
 }
};
}
