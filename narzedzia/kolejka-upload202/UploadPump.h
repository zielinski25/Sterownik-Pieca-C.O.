#pragma once
#include "RequestRegistry.h"
namespace upload202 {
enum class Admission { Accepted, Rejected, Stale };
enum class Step { Idle, Worked, Busy };
template<class Registry,std::size_t Requests=2,std::size_t Frames=16,std::size_t Bytes=256>class Pump {
 static_assert(Requests&&Frames&&Bytes&&Frames<=std::numeric_limits<std::size_t>::max()/Bytes,"bounded buffers");
public:using Token=typename Registry::Token;
private:
 struct Request {
  bool occupied=false,connected=false,busy=false,ended=false,done=false,replyReady=false,delivered=false;
  uintptr_t key=0;Token token{};std::size_t pending=0;int failure=0;upload200::Reply reply{};
 } requests_[Requests];
 struct Frame {Token token{};char name[64]{};std::size_t nameLength=0,index=0,length=0;bool final=false;unsigned char data[Bytes]{};} frames_[Frames];
 std::size_t count_=0;bool running_=false,cleanup_=false;Registry&registry_;history197::Mutex&mutex_;
 struct Guard {history197::Mutex&m;explicit Guard(history197::Mutex&x):m(x){m.lock();}~Guard(){m.unlock();}};
 static bool equal(Token a,Token b){return a.slot==b.slot&&a.generation==b.generation;}
 Request*find(Token t){for(auto&r:requests_)if(r.occupied&&equal(r.token,t))return &r;return nullptr;}
 void purge(Request&r){std::size_t out=0;for(std::size_t i=0;i<count_;++i){if(equal(frames_[i].token,r.token))--r.pending;else frames_[out++]=frames_[i];}count_=out;}
 void fail(Request&r,int status){r.failure=status;purge(r);registry_.disconnect(r.token);cleanup_=true;if(r.ended){r.reply={true,status,0};r.replyReady=true;r.done=true;}}
 void retire(Request&r){if(!r.connected&&!r.busy)r.occupied=false;}
public:
 Pump(Registry&r,history197::Mutex&m):registry_(r),mutex_(m){}
 ~Pump()=delete;Pump(const Pump&)=delete;Pump&operator=(const Pump&)=delete;
 Token attach(uintptr_t key,std::size_t expected,bool authorized){Guard g(mutex_);Token invalid{std::numeric_limits<std::size_t>::max(),0};
  if(!key)return invalid;
  for(const auto&r:requests_)if(r.occupied&&r.key==key)return invalid;
  Request*slot=nullptr;for(auto&r:requests_)if(!r.occupied){slot=&r;break;}if(!slot)return invalid;
  Token t=registry_.attach(key,expected,authorized);if(!t.generation)return invalid;
  *slot=Request{};slot->occupied=true;slot->connected=true;slot->key=key;slot->token=t;return t;
 }
 Admission enqueue(Token t,const char*name,std::size_t nameLength,std::size_t index,const unsigned char*data,std::size_t len,bool final){Guard g(mutex_);auto*r=find(t);if(!r||!r->connected)return Admission::Stale;
  if(r->failure||r->done)return Admission::Rejected;
  if(r->ended||!name||!nameLength||nameLength>63||(!data&&len)||index>std::numeric_limits<std::size_t>::max()-len){fail(*r,400);return Admission::Rejected;}
  const std::size_t parts=len?len/Bytes+(len%Bytes!=0):1;
  if(parts>Frames-count_){fail(*r,503);return Admission::Rejected;} // all-or-nothing reservation
  std::size_t at=0;for(std::size_t i=0;i<parts;++i){auto&f=frames_[count_++];f=Frame{};f.token=t;f.nameLength=nameLength;std::memcpy(f.name,name,nameLength);f.index=index+at;f.length=std::min(Bytes,len-at);f.final=final&&i+1==parts;if(f.length)std::memcpy(f.data,data+at,f.length);at+=f.length;++r->pending;}
  return Admission::Accepted;
 }
 bool end(Token t){Guard g(mutex_);auto*r=find(t);if(!r||!r->connected||r->ended)return false;r->ended=true;if(r->failure){r->reply={true,r->failure,0};r->replyReady=true;r->done=true;}return true;}
 bool disconnect(Token t){Guard g(mutex_);auto*r=find(t);if(!r||!r->connected)return false;r->connected=false;r->replyReady=false;purge(*r);registry_.disconnect(t);cleanup_=true;retire(*r);return true;}
 // Only a command/receipt; caller still needs a live request in the network context.
 upload200::Reply takeReply(Token t){Guard g(mutex_);auto*r=find(t);if(!r||!r->connected||!r->replyReady||r->delivered)return {false,0,0};r->delivered=true;return r->reply;}
 Step runOne(){
  Frame work;Token token{};bool cleanup=false,completion=false;
  {Guard g(mutex_);if(running_)return Step::Busy;
   if(cleanup_){cleanup_=false;cleanup=true;}
   else if(count_){work=frames_[0];for(std::size_t i=1;i<count_;++i)frames_[i-1]=frames_[i];--count_;token=work.token;auto*r=find(token);r->busy=true;}
   else{Request*ready=nullptr;for(auto&r:requests_)if(r.occupied&&r.connected&&r.ended&&!r.done&&!r.failure&&!r.pending&&!r.busy){ready=&r;break;}if(!ready)return Step::Idle;ready->busy=true;token=ready->token;completion=true;}
   running_=true;
  }
  if(cleanup){const bool more=registry_.service();Guard g(mutex_);cleanup_=cleanup_||more;running_=false;return Step::Worked;}
  auto result=completion?registry_.complete(token):registry_.chunk(token,work.name,work.nameLength,work.index,work.data,work.length,work.final);
  {Guard g(mutex_);auto*r=find(token);
   if(r){r->busy=false;if(!completion)--r->pending;
    if(r->connected){
     if(r->failure){if(r->ended){r->reply={true,r->failure,0};r->replyReady=true;r->done=true;}}
     else if(completion){r->done=true;if(result.action==upload201::Action::Emit){r->reply={true,result.status,result.bytes};r->replyReady=true;}else fail(*r,503);}
     else if(result.action!=upload201::Action::Accepted)fail(*r,result.status?result.status:503);
    }retire(*r);
   }running_=false;
  }return Step::Worked;
 }
};
}
