#pragma once
#include "RuntimeFilesystem.h"
#include "Engine198.h"

namespace history198 {
struct Result {
 bool admitted, confirmed, ready;
 history193::Fault fault;
 int error;
 std::size_t unresolved;
};
// Caller still needs its application lock (e.g. logMutex) BEFORE this API.
// Owner, raw Calls, mutex, this object and their storage live until reset.
// This is NOT a boot/provisioning interface and exposes no raw Calls or Engine.
template<class Owner, std::size_t Capacity=2> class RuntimeHistory {
 static_assert(Capacity>0,"a descriptor reservation is required");
 Owner& owner_;
 class Gate final:public history193::Calls {
  enum State { Free, Live, CloseAttempted };
  struct Slot { int fd=-1; State state=Free; } slots_[Capacity];
  Owner& owner_; history193::Calls& raw_; int error_=0;
  int deny(){error_=ECANCELED;return -1;}
  int poison(int error){error_=error?error:EIO;owner_.trip();return -1;}
  Slot* find(int fd){for(auto&s:slots_)if(s.state==Live&&s.fd==fd)return &s;return nullptr;}
  bool permitted(int fd){if(!owner_.ready()){deny();return false;}if(!find(fd)){poison(EBADF);return false;}return true;}
 public:
  Gate(Owner&owner,history193::Calls&raw):owner_(owner),raw_(raw){}
  int open(const char*p,int flags)override{
   if(!owner_.ready())return deny();
   Slot*slot=nullptr;for(auto&s:slots_)if(s.state==Free){slot=&s;break;}
   if(!slot)return poison(EMFILE); // reserve BEFORE raw open
   slot->state=Live;slot->fd=-1;
   const int fd=raw_.open(p,flags);const int e=raw_.error();
   if(fd<0){slot->state=Free;return poison(e);}
   slot->fd=fd; // retain even positive-open + errno or stop during open
   if(e||!owner_.ready())return poison(e?e:ECANCELED);
   error_=0;return fd;
  }
  int stat(int fd,struct stat*s)override{
   if(!permitted(fd))return -1;
   const int r=raw_.stat(fd,s);const int e=raw_.error();
   if(r!=0||(e&&e!=ENODATA)||!owner_.ready())return poison(e?e:EIO);
   error_=e;return r;
  }
  off_t seek(int fd,off_t pos)override{
   if(!permitted(fd))return -1;
   const off_t r=raw_.seek(fd,pos);const int e=raw_.error();
   if(r!=pos||e||!owner_.ready())return poison(e);
   error_=0;return r;
  }
  ssize_t read(int fd,void*p,std::size_t n)override{
   if(!permitted(fd))return -1;
   const ssize_t r=raw_.read(fd,p,n);const int e=raw_.error();
   if(r<0||static_cast<std::size_t>(r)!=n||e||!owner_.ready())return poison(e);
   error_=0;return r;
  }
  ssize_t write(int fd,const void*p,std::size_t n)override{
   if(!permitted(fd))return -1;
   const ssize_t r=raw_.write(fd,p,n);const int e=raw_.error();
   if(r<0||static_cast<std::size_t>(r)!=n||e||!owner_.ready())return poison(e);
   error_=0;return r;
  }
  int sync(int fd)override{
   if(!permitted(fd))return -1;
   const int r=raw_.sync(fd);const int e=raw_.error();
   if(r!=0||e||!owner_.ready())return poison(e);
   error_=0;return r;
  }
  int close(int fd)override{
   if(!permitted(fd))return -1;
   Slot*slot=find(fd);slot->state=CloseAttempted; // never retry this close
   const int r=raw_.close(fd);const int e=raw_.error();
   if(r!=0||e||!owner_.ready())return poison(e);
   slot->state=Free;slot->fd=-1;error_=0;return 0;
  }
  int rename(const char*a,const char*b)override{
   if(!owner_.ready())return deny();
   const int r=raw_.rename(a,b);const int e=raw_.error();
   if(r!=0||e||!owner_.ready())return poison(e);
   error_=0;return 0;
  }
  int error()const override{return error_;}
  std::size_t unresolved()const{std::size_t n=0;for(const auto&s:slots_)if(s.state!=Free)++n;return n;}
 } gate_;
 void(*observer_)(void*); void*context_;
 history198core::Engine engine_;
 static void failed(void*context){auto&self=*static_cast<RuntimeHistory*>(context);self.owner_.trip();if(self.observer_)self.observer_(self.context_);}
 Result result(bool admitted,bool confirmed)const{return {admitted,confirmed,owner_.ready(),engine_.fault(),engine_.errorNumber(),gate_.unresolved()};}
public:
 explicit RuntimeHistory(Owner&owner,history193::Calls&raw,void(*service)()=nullptr,void(*observer)(void*)=nullptr,void*context=nullptr)
  :owner_(owner),gate_(owner,raw),observer_(observer),context_(context),engine_(gate_,service,failed,this){}
 ~RuntimeHistory()=delete;
 RuntimeHistory(const RuntimeHistory&)=delete;RuntimeHistory&operator=(const RuntimeHistory&)=delete;
 Result append(history193::Stream stream,const char*data,std::size_t n,std::uint64_t&size){
  typename Owner::Lease lease(owner_);if(!lease)return result(false,false);
  const bool confirmed=engine_.append(stream,data,n,size);
  // A later stop cannot retract a transaction already confirmed by Engine.
  return result(true,confirmed);
 }
 Result trim(history193::Stream stream,unsigned keep,std::uint64_t&size,unsigned&lines){
  typename Owner::Lease lease(owner_);if(!lease)return result(false,false);
  const bool confirmed=engine_.trim(stream,keep,size,lines);return result(true,confirmed);
 }
 Result snapshot(){typename Owner::Lease lease(owner_);return result(false,false);}
};
}
