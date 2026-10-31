#pragma once
#include <memory>
#include <cstdint>
#ifndef HISTORY203_DEFERRED_POLL
#error "DeferredUploadReply requires the 203 request patch"
#endif
namespace upload203 {
// Services::now() returns uint32_t ms; makeResponse(status) returns owned response
// or null. Services and this context remain alive through request disconnect.
// Every method here runs ONLY in the serialized request/network context.
template<class Pump,class Services>class DeferredUploadReply {
 Pump&pump_;typename Pump::Token token_;uintptr_t key_;Services&services_;
 uint32_t started_=0,timeout_=0;bool bound_=false,done_=false,disconnected_=false;AsyncWebServerRequestPtr request_;
 static void poll(AsyncWebServerRequest*request,void*context){static_cast<DeferredUploadReply*>(context)->tick(*request);}
 static void gone(void*context){auto&self=*static_cast<DeferredUploadReply*>(context);self.done_=true;self.disconnected_=true;self.pump_.disconnect(self.token_);}
 void stop(AsyncWebServerRequest&r){done_=true;r.stopDeferredPoll203();pump_.disconnect(token_);}
 void tick(AsyncWebServerRequest&r){
  if(done_)return;
  if(r.isSent()||r.getResponse()){stop(r);return;}
  if(static_cast<uint32_t>(services_.now()-started_)>=timeout_){stop(r);r.abort();return;}
  if(!r.client()||!r.client()->canSend())return;
  auto reply=pump_.takeReply(token_);if(!reply.emit)return;
  std::unique_ptr<AsyncWebServerResponse>response(services_.makeResponse(reply.status));
  // Factory may have caused a disconnect or another response; do not leak or replace it.
  if(done_||r.disconnected203()||r.isSent()||r.getResponse()){stop(r);return;}
  if(!response){stop(r);r.abort();return;}
  done_=true;r.stopDeferredPoll203();r.send(response.release());
 }
public:
 DeferredUploadReply(Pump&p,typename Pump::Token token,uintptr_t key,Services&s):pump_(p),token_(token),key_(key),services_(s){}
 ~DeferredUploadReply()=delete;DeferredUploadReply(const DeferredUploadReply&)=delete;DeferredUploadReply&operator=(const DeferredUploadReply&)=delete;
 // Reuse only after the old disconnect AND actual request destruction.
 bool rearm(typename Pump::Token token,uintptr_t key){
  if(!bound_||!done_||!disconnected_||!request_.expired()||!token.generation||!key)return false;
  request_.reset();token_=token;key_=key;bound_=false;done_=false;disconnected_=false;return true;
 }
 bool bind(AsyncWebServerRequest&r,uint32_t timeout){
  if(bound_||!token_.generation||!timeout||timeout>0x7fffffffU||reinterpret_cast<uintptr_t>(&r)!=key_||r.isSent()||r.isPaused()||r.getResponse())return false;
  auto keep=r.shared_from_this();if(!keep)return false;
  if(!r.installDeferred203(poll,gone,this))return false;
  request_=keep;bound_=true;started_=services_.now();timeout_=timeout;
  auto weak=r.pause();return !weak.expired()&&!done_;
 }
};
}
