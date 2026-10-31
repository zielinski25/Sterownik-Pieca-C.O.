#pragma once
// Mechanically extracted 193 Engine; only observer context plumbing changed.
// Cleanup calls are requests: the enclosing gate may suppress actual backend close.
#include "CheckedHistoryIO.h"
namespace history198core {
using history193::Calls;using history193::Fault;using history193::Stream;
class Engine {
 Calls&io_;void(*service_)();void(*faultObserver_)(void*);void*faultContext_;Fault fault_=Fault::None;int error_=0;
 static constexpr std::size_t chunk=256;
 static const char*name(Stream s){return s==Stream::Short?"/littlefs/hist_short.csv":"/littlefs/hist_long.csv";}
 static const char*stage(Stream s){return s==Stream::Short?"/littlefs/hist_short.csv.rebuild":"/littlefs/hist_long.csv.rebuild";}
 bool fail(Fault f){if(fault_==Fault::None){fault_=f;error_=io_.error();if(faultObserver_)faultObserver_(faultContext_);}return false;}
 void pulse(){if(service_)service_();}
 int open(const char*p,int flags){int f=io_.open(p,flags);if(f<0||io_.error()!=0)fail(Fault::Open);return f;}
 bool size(int f,off_t&n){struct stat st{};if(io_.stat(f,&st)!=0||(io_.error()!=0&&io_.error()!=ENODATA)||!S_ISREG(st.st_mode)||st.st_size<0)return fail(Fault::Stat);n=st.st_size;return true;}
 bool seek(int f,off_t p){return (io_.seek(f,p)==p&&io_.error()==0)||fail(Fault::Seek);}
 bool readAt(int f,off_t p,void*b,std::size_t n){
  if(!seek(f,p))return false;
  if(io_.read(f,b,n)!=static_cast<ssize_t>(n)||io_.error()!=0)return fail(Fault::Read);
  pulse();return true;
 }
 bool writeAt(int f,off_t p,const void*b,std::size_t n){
  if(!seek(f,p))return false;
  // Pinned wrapper may hide an auto-sync error behind a positive byte count.
  if(io_.write(f,b,n)!=static_cast<ssize_t>(n)||io_.error()!=0)return fail(Fault::Write);
  pulse();return true;
 }
 bool sync(int f){return (io_.sync(f)==0&&io_.error()==0)||fail(Fault::Sync);}
 bool close(int&f){if(f<0)return !blocked();int owned=f;f=-1;if(io_.close(owned)!=0||io_.error()!=0)return fail(Fault::Close);return !blocked();}
public:
 explicit Engine(Calls&io,void(*service)()=nullptr,void(*observer)(void*)=nullptr,void*context=nullptr):io_(io),service_(service),faultObserver_(observer),faultContext_(context){}
 Engine(const Engine&)=delete;Engine&operator=(const Engine&)=delete;
 bool blocked()const{return fault_!=Fault::None;}
 Fault fault()const{return fault_;}int errorNumber()const{return error_;}
 bool append(Stream stream,const char*data,std::size_t n,std::uint64_t&resultSize){
  if(blocked())return false;
  if(!data||!n||n>4096||data[n-1]!='\n'||std::memchr(data,0,n))return fail(Fault::Input);
  int fd=open(name(stream),O_RDWR|O_CREAT);off_t base=0;
  bool ok=!blocked()&&size(fd,base);
  if(ok&&n>static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()-base))ok=fail(Fault::Input);
  char buffer[chunk];
  if(ok&&base){ok=readAt(fd,base-1,buffer,1);if(ok&&buffer[0]!='\n')ok=fail(Fault::Boundary);}
  for(std::size_t at=0;ok&&at<n;){std::size_t count=n-at<chunk?n-at:chunk;ok=writeAt(fd,base+at,data+at,count);at+=count;}
  if(ok)ok=sync(fd);
  off_t end=0;if(ok){ok=size(fd,end);if(ok&&end!=base+static_cast<off_t>(n))ok=fail(Fault::Verify);}
  for(std::size_t at=0;ok&&at<n;){std::size_t count=n-at<chunk?n-at:chunk;ok=readAt(fd,base+at,buffer,count);if(ok&&std::memcmp(buffer,data+at,count)!=0)ok=fail(Fault::Verify);at+=count;}
  const bool closed=close(fd);
  if(!ok||!closed)return false;
  resultSize=static_cast<std::uint64_t>(end);return true;
 }
 // Retain the last keep complete physical lines from the COMMITTED FILE,
 // never from a changing RAM ring (which may include still-pending rows).
 // The target is never predeleted or opened with TRUNC. Stale stage => stop.
 bool trim(Stream stream,unsigned keep,std::uint64_t&resultSize,unsigned&resultLines){
  if(blocked())return false;
  if(!keep)return fail(Fault::Input);
  int source=open(name(stream),O_RDONLY),temp=-1;off_t total=0,start=0;
  bool ok=!blocked()&&size(source,total);char buffer[chunk],verify[chunk];
  if(ok&&total){ok=readAt(source,total-1,buffer,1);if(ok&&buffer[0]!='\n')ok=fail(Fault::Boundary);}
  unsigned found=0;off_t cursor=total;bool boundary=false;
  while(ok&&cursor>0&&!boundary){std::size_t count=cursor<static_cast<off_t>(chunk)?static_cast<std::size_t>(cursor):chunk;cursor-=count;ok=readAt(source,cursor,buffer,count);
   if(ok)for(std::size_t j=count;j>0;--j)if(buffer[j-1]=='\n'){
    if(found==keep){start=cursor+j;boundary=true;break;}++found;
   }
  }
  const off_t length=total-start;
  if(ok&&start>0){
   temp=open(stage(stream),O_RDWR|O_CREAT|O_EXCL);ok=!blocked();off_t initial=0;
   if(ok){ok=size(temp,initial);if(ok&&initial!=0)ok=fail(Fault::Verify);}
   for(off_t at=0;ok&&at<length;){std::size_t count=length-at<static_cast<off_t>(chunk)?static_cast<std::size_t>(length-at):chunk;ok=readAt(source,start+at,buffer,count)&&writeAt(temp,at,buffer,count);at+=count;}
   if(ok)ok=sync(temp);
   off_t end=0;if(ok){ok=size(temp,end);if(ok&&end!=length)ok=fail(Fault::Verify);}
   for(off_t at=0;ok&&at<length;){std::size_t count=length-at<static_cast<off_t>(chunk)?static_cast<std::size_t>(length-at):chunk;ok=readAt(source,start+at,buffer,count)&&readAt(temp,at,verify,count);if(ok&&std::memcmp(buffer,verify,count)!=0)ok=fail(Fault::Verify);at+=count;}
   off_t unchanged=0;if(ok){ok=size(source,unchanged);if(ok&&unchanged!=total)ok=fail(Fault::Verify);}
  }
  // Always attempt each owned close once, including after the first error.
  const bool tempClosed=close(temp);const bool sourceClosed=close(source);
  if(!ok||!tempClosed||!sourceClosed)return false;
  if(start>0&&(io_.rename(stage(stream),name(stream))!=0||io_.error()!=0))return fail(Fault::Rename);
  resultSize=static_cast<std::uint64_t>(length);resultLines=found;return true;
 }
};
}
