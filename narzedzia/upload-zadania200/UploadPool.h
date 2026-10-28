#pragma once
#include "RuntimeFilesystem.h"
#include "CheckedHistoryIO.h"
#include <limits>
#include <algorithm>
#include <type_traits>
namespace upload200 {
struct Reply {bool emit;int status;std::size_t verified;};
// Request metadata/lifetime binding is deliberately outside this offline component.
// All methods take Owner's nonrecursive Lease. Never call them while holding Lease.
template<class Owner,std::size_t Slots=2,std::size_t MaxBytes=4096,class Epoch=uint64_t>
class Pool {
 static_assert(Slots>0&&MaxBytes>0,"bounded pool and quota required");
 static_assert(std::is_unsigned<Epoch>::value&&std::numeric_limits<Epoch>::digits>=8,"unsigned generation");
 enum Phase { Receiving, FinalSeen, Failed, Confirmed };
 enum Descriptor { None, Open, CloseAttempted };
 struct Slot {
  bool occupied=false,emitted=false;Epoch generation=0;
  Phase phase=Receiving;Descriptor descriptor=None;int fd=-1,status=0;
  std::size_t expected=0,verified=0;char path[80]{};
 } slots_[Slots];
 Owner&owner_;history193::Calls&raw_;
 static bool nameAllowed(const char*p,std::size_t n){
  if(!p||!n||n>63||p[0]=='.')return false;
  for(std::size_t i=0;i<n;++i){unsigned char c=p[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;if(i&&c=='.'&&p[i-1]=='.')return false;}
  const char*protectedNames[]={"hist_short.csv","hist_long.csv","hist_short.csv.rebuild","hist_long.csv.rebuild","history_reset_a1.req","history_reset_a1.done","log_a.txt","log_b.txt","log_krytyczny.txt","panel_firmware.bin"};
  for(const char*s:protectedNames)if(std::strlen(s)==n&&!std::memcmp(p,s,n))return false;
  return true;
 }
 bool failIO(Slot&s){s.phase=Failed;s.status=500;owner_.trip();return false;}
 bool check(Slot&s,bool ok,bool allowNoData=false){const int e=raw_.error();return (ok&&(!e||(allowNoData&&e==ENODATA))&&owner_.ready())||failIO(s);}
 bool active(Slot&s){if(owner_.ready())return true;s.phase=Failed;s.status=503;return false;}
 bool close(Slot&s){
  if(s.descriptor==None)return true;
  if(s.descriptor!=Open||!active(s))return false;
  s.descriptor=CloseAttempted;const int r=raw_.close(s.fd);
  if(!check(s,r==0))return false;
  s.descriptor=None;s.fd=-1;return true;
 }
 void reject(Slot&s,int status){s.phase=Failed;s.status=status;close(s);}
 bool seek(Slot&s,off_t position){if(!active(s))return false;const off_t got=raw_.seek(s.fd,position);return check(s,got==position);}
 bool sync(Slot&s){if(!active(s))return false;const int got=raw_.sync(s.fd);return check(s,got==0);}
 bool write(Slot&s,const unsigned char*data,std::size_t len){
  unsigned char verify[256];
  for(std::size_t at=0;at<len;){
   const std::size_t count=std::min(sizeof(verify),len-at);
   if(!active(s))return false;
   const ssize_t got=raw_.write(s.fd,data+at,count);
   if(!check(s,got==static_cast<ssize_t>(count))||!sync(s)||!seek(s,static_cast<off_t>(s.verified)))return false;
   if(!active(s))return false;
   const ssize_t read=raw_.read(s.fd,verify,count);
   if(!check(s,read==static_cast<ssize_t>(count)))return false;
   if(std::memcmp(verify,data+at,count))return failIO(s);
   s.verified+=count;at+=count;
  }return true;
 }
public:
 struct Ticket {std::size_t slot;Epoch generation;};
private:
 Slot*find(Ticket t){if(t.slot>=Slots)return nullptr;auto&s=slots_[t.slot];return s.occupied&&s.generation==t.generation?&s:nullptr;}
public:
 Pool(Owner&owner,history193::Calls&raw):owner_(owner),raw_(raw){}
 ~Pool()=delete;Pool(const Pool&)=delete;Pool&operator=(const Pool&)=delete;
 // Invalid ticket means no capacity: caller must keep its own per-request 503.
 Ticket begin(const char*name,std::size_t length,std::size_t expected){
  typename Owner::Lease lease(owner_);
  std::size_t i=0;for(;i<Slots;++i)if(!slots_[i].occupied&&slots_[i].generation<std::numeric_limits<Epoch>::max())break;
  if(i==Slots)return {Slots,0};
  auto&s=slots_[i];const Epoch next=s.generation+1;s=Slot{};s.occupied=true;s.generation=next;s.expected=expected;Ticket t{i,next};
  if(!nameAllowed(name,length)){s.phase=Failed;s.status=400;return t;}
  if(expected>MaxBytes||expected>static_cast<uint64_t>(std::numeric_limits<off_t>::max())){s.phase=Failed;s.status=413;return t;}
  if(!lease){s.phase=Failed;s.status=503;return t;}
  std::memcpy(s.path,"/littlefs/",10);std::memcpy(s.path+10,name,length);s.path[10+length]=0;
  s.fd=raw_.open(s.path,O_RDWR|O_CREAT|O_EXCL);const int e=raw_.error();
  if(s.fd>=0)s.descriptor=Open; // retained even when errno reports an error
  if(s.fd<0&&e==EEXIST&&owner_.ready()){s.phase=Failed;s.status=409;return t;}
  if(s.fd<0||e||!owner_.ready())failIO(s);
  return t;
 }
 bool chunk(Ticket t,std::size_t index,const unsigned char*data,std::size_t len,bool final){
  typename Owner::Lease lease(owner_);auto*p=find(t);if(!p)return false;auto&s=*p;
  if(s.phase==Failed||s.phase==Confirmed)return false;
  if(!lease){active(s);return false;}
  if(s.phase!=Receiving||index!=s.verified||s.verified>s.expected||len>s.expected-s.verified||(!data&&len)||(!len&&!final)){reject(s,400);return false;}
  if(!write(s,data,len))return false;
  if(final){if(s.verified!=s.expected){reject(s,400);return false;}s.phase=FinalSeen;}
  return true;
 }
 // Called only by the successful end-of-whole-request event, not file final=true.
 Reply complete(Ticket t){
  typename Owner::Lease lease(owner_);auto*p=find(t);if(!p)return {false,0,0};auto&s=*p;
  if(s.emitted)return {false,s.status,s.verified};
  if(s.phase!=Failed&&s.phase!=Confirmed){
   if(!lease)active(s);
   else if(s.phase!=FinalSeen)reject(s,400);
   else if(sync(s)){
    struct stat st{};
    if(active(s)){
     const int got=raw_.stat(s.fd,&st);
     if(check(s,got==0,true)){
      if(!S_ISREG(st.st_mode)||st.st_size<0||static_cast<uint64_t>(st.st_size)!=s.verified)failIO(s);
      else if(close(s)){s.phase=Confirmed;s.status=200;}
     }
    }
   }
  }
  s.emitted=true;return {true,s.status,s.verified};
 }
 // Disconnect/cancel may close a healthy partial NEW file, but never delete it.
 // Caller must schedule this in the qualified FS context, not arbitrary callbacks.
 bool release(Ticket t){
  typename Owner::Lease lease(owner_);auto*p=find(t);if(!p)return false;auto&s=*p;
  if(s.phase!=Failed&&s.phase!=Confirmed)reject(s,400);
  if(s.descriptor!=None)return false; // quarantine; do not forget uncertain FD
  s.occupied=false;return true;
 }
 std::size_t retained(){typename Owner::Lease lease(owner_);std::size_t n=0;for(const auto&s:slots_)if(s.occupied&&s.descriptor!=None)++n;return n;}
};
}
