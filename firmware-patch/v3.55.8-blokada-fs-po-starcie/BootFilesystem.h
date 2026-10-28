#pragma once
#include <FS.h>
#include <atomic>
namespace history196 {
enum class Phase { Cold, Mounting, Checking, Ready, Blocked };
// Boot-only publication. Ready is immutable: this is NOT runtime quarantine.
// No real FS/File reference escapes before a successful finish().
template<class Backend> class BootFilesystem {
 Backend&backend_;fs::FS empty_{fs::FSImplPtr()};
 std::atomic<Phase> phase_{Phase::Cold};
 fs::FS& selected(){return ready()?static_cast<fs::FS&>(backend_):empty_;}
public:
 explicit BootFilesystem(Backend&backend):backend_(backend){}
 BootFilesystem(const BootFilesystem&)=delete;
 BootFilesystem&operator=(const BootFilesystem&)=delete;
 bool ready()const{return phase_.load(std::memory_order_acquire)==Phase::Ready;}
 Phase phase()const{return phase_.load(std::memory_order_acquire);}
 bool begin(bool formatOnFail=false){
  Phase old=Phase::Cold;
  if(!phase_.compare_exchange_strong(old,Phase::Mounting))return false;
  if(formatOnFail||!backend_.begin(false)){phase_.store(Phase::Blocked,std::memory_order_release);return false;}
  phase_.store(Phase::Checking,std::memory_order_release);
  return true;
 }
 // Sole boot owner calls finish after ALL privileged boot I/O has stopped.
 bool finish(bool bootOK){Phase old=Phase::Checking;
  if(!phase_.compare_exchange_strong(old,bootOK?Phase::Ready:Phase::Blocked,std::memory_order_acq_rel))return false;
  return bootOK;
 }
 operator fs::FS&(){return selected();}
 template<class Path> fs::File open(const Path&p,const char*mode=FILE_READ,bool create=false){return selected().open(p,mode,create);}
 template<class Path> bool exists(const Path&p){return selected().exists(p);}
 template<class Path> bool remove(const Path&p){return selected().remove(p);}
 template<class A,class B> bool rename(const A&a,const B&b){return selected().rename(a,b);}
 template<class Path> bool mkdir(const Path&p){return selected().mkdir(p);}
 template<class Path> bool rmdir(const Path&p){return selected().rmdir(p);}
 const char*mountpoint(){return selected().mountpoint();}
 size_t totalBytes(){return ready()?backend_.totalBytes():0;}
 size_t usedBytes(){return ready()?backend_.usedBytes():0;}
 bool format()=delete;
 void end()=delete;
};
}
