#pragma once
#include "CheckedHistoryIO.h"
#include <atomic>
namespace history194 {
enum class State { Cold, Checking, Ready, Blocked };
enum class Reason { None, Mount, IO, Stage, Request, Completion, Boundary, Runtime };
struct Sizes { uint64_t shortBytes=0,longBytes=0; };
// Sole boot caller; concurrent readers observe only atomic publication.
// No delete/create/write/sync/rename, no recovery, no automatic A1.
class Startup {
 history193::Calls&io_;
 std::atomic<State> state_{State::Cold};
 std::atomic<Reason> reason_{Reason::None};
 std::atomic<int> error_{0};
 bool fail(Reason why){Reason old=Reason::None;if(reason_.compare_exchange_strong(old,why)){error_.store(io_.error());}state_.store(State::Blocked,std::memory_order_release);return false;}
 bool close(int fd){if(io_.close(fd)!=0||io_.error()!=0)return fail(Reason::IO);return true;}
 // -1 absent, -2 error. Positive-open/errno is ambiguous: close ONCE.
 int open(const char*p){int fd=io_.open(p,O_RDONLY);int err=io_.error();if(fd<0){if(err==ENOENT)return -1;fail(Reason::IO);return -2;}if(err){fail(Reason::IO);close(fd);return -2;}return fd;}
 bool absent(const char*p,Reason why){int fd=open(p);if(fd==-1)return true;if(fd==-2)return false;fail(why);close(fd);return false;}
 bool completion(){int fd=open("/littlefs/history_reset_a1.done");if(fd==-1)return true;if(fd==-2)return false;
  const char expected[]="HISTORY-A1-COMPLETE\n";char b[sizeof(expected)]{};
  const auto n=io_.read(fd,b,sizeof(b));bool ok=true;
  if(n<0||io_.error()!=0)ok=fail(Reason::IO);
  else if(n!=sizeof(expected)-1||std::memcmp(b,expected,sizeof(expected)-1))ok=fail(Reason::Completion);
  bool closed=close(fd);return ok&&closed;
 }
 bool history(const char*p,uint64_t&bytes){int fd=open(p);if(fd==-1){bytes=0;return true;}if(fd==-2)return false;
  struct stat s{};bool ok=true;
  if(io_.stat(fd,&s)!=0||(io_.error()!=0&&io_.error()!=ENODATA)||!S_ISREG(s.st_mode)||s.st_size<0||static_cast<uint64_t>(s.st_size)>std::numeric_limits<size_t>::max())ok=fail(Reason::IO);
  if(ok&&s.st_size){char last=0;if(io_.seek(fd,s.st_size-1)!=s.st_size-1||io_.error()!=0)ok=fail(Reason::IO);
   if(ok){if(io_.read(fd,&last,1)!=1||io_.error()!=0)ok=fail(Reason::IO);else if(last!='\n')ok=fail(Reason::Boundary);}}
  const bool closed=close(fd);if(!ok||!closed)return false;bytes=static_cast<uint64_t>(s.st_size);return true;
 }
public:
 explicit Startup(history193::Calls&io):io_(io){}
 bool ready()const{return state_.load(std::memory_order_acquire)==State::Ready;}
 State state()const{return state_.load(std::memory_order_acquire);}
 Reason reason()const{return reason_.load(std::memory_order_acquire);}
 int errorNumber()const{return error_.load();}
 void runtimeFault(){Reason old=Reason::None;reason_.compare_exchange_strong(old,Reason::Runtime);state_.store(State::Blocked,std::memory_order_release);}
 bool start(bool mounted,Sizes&out){State old=State::Cold;if(!state_.compare_exchange_strong(old,State::Checking))return false;
  if(!mounted)return fail(Reason::Mount);
  Sizes sizes;
  if(!absent("/littlefs/hist_short.csv.rebuild",Reason::Stage)||!absent("/littlefs/hist_long.csv.rebuild",Reason::Stage)||!absent("/littlefs/history_reset_a1.req",Reason::Request)||!completion()||!history("/littlefs/hist_short.csv",sizes.shortBytes)||!history("/littlefs/hist_long.csv",sizes.longBytes))return false;
  // Boot is sole writer of out. Concurrent runtimeFault cannot reopen gate.
  old=State::Checking;
  if(state_.load()!=old)return false;
  const Sizes previous=out;out=sizes;
  const bool ok=state_.compare_exchange_strong(old,State::Ready,std::memory_order_release);
  if(!ok)out=previous;
  return ok;
 }
};
}
