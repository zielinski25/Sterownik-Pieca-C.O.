#pragma once
#include "Transition.h"
#include <cstddef>
#include <cstring>
namespace history_a191 {
using namespace history_a190;
enum class Path { Request, Completion, Short, Long };
enum class Open { Read, ExistingReadWrite, ExclusiveCreate };
// Exactly one open handle, owned by the boot owner. 0=success, -2=ENOENT,
// -1=other error. stat rejects non-regular files. No retry of ambiguous close.
struct Calls {
 virtual ~Calls()=default;
 virtual int stat(Path)=0;
 virtual int open(Path,Open)=0;
 virtual int read(void*,std::size_t)=0;
 virtual int write(const void*,std::size_t)=0;
 virtual bool rewind()=0;
 virtual bool sync()=0;
 virtual bool close()=0;
 virtual bool remove(Path)=0;
};
// Requires a LittleFS backend with synchronous metadata commit semantics.
// NOT a generic POSIX replacement: in particular, do not drop directory fsync
// from the POSIX adapter190 and assume these guarantees on another filesystem.
class Storage final:public history_a190::Storage {
 Calls&io_;bool fault_=false;
 static constexpr const char* requestText="RESET-HISTORY-A1\n";
 static constexpr const char* completeText="HISTORY-A1-COMPLETE\n";
 bool finish(bool ok){const bool closed=io_.close();fault_=!ok||!closed;return !fault_;}
 Token readOpen(const char*text){
  char b[64]{};std::size_t n=0;bool eof=false;
  while(n<sizeof(b)){
   int got=io_.read(b+n,sizeof(b)-n);
   if(got<0||static_cast<std::size_t>(got)>sizeof(b)-n)return Token::Fault;
   if(!got){eof=true;break;}n+=got;
  }
  return eof&&n==std::strlen(text)&&std::memcmp(b,text,n)==0?Token::Valid:Token::Invalid;
 }
 Token token(Path p,const char*text){
  if(fault_)return Token::Fault;
  int r=io_.open(p,Open::Read);
  if(r==-2)return Token::Absent;
  if(r){fault_=true;return Token::Fault;}
  auto t=readOpen(text);if(!finish(t!=Token::Fault))return Token::Fault;return t;
 }
 bool erase(Path p){
  if(fault_)return false;
  int r=io_.stat(p);if(r==-2)return true;
  if(r||!io_.remove(p)||io_.stat(p)!=-2){fault_=true;return false;}return true;
 }
public:
 explicit Storage(Calls&io):io_(io){}
 Token completion()override{return token(Path::Completion,completeText);}
 Token request()override{return token(Path::Request,requestText);}
 bool removeHistory(History h)override{return erase(h==History::Short?Path::Short:Path::Long);}
 bool consumeRequest()override{return erase(Path::Request);}
 bool createCompletion()override{
  if(fault_)return false;
  if(io_.open(Path::Completion,Open::ExclusiveCreate)){fault_=true;return false;}
  const auto n=std::strlen(completeText);
  bool ok=io_.write(completeText,n)==static_cast<int>(n);
  if(ok)ok=io_.sync();
  return finish(ok);
 }
 bool sealCompletion()override{
  if(fault_)return false;
  if(io_.open(Path::Completion,Open::ExistingReadWrite)){fault_=true;return false;}
  // sync on a clean/read-only LittleFS file need not reach block-device sync.
  // Re-write the SAME bytes, without create/truncate, to dirty the file and
  // force a real checked metadata commit. A failed operation remains a fault
  // even if subsequent close of an LFS_F_ERRED file returns zero.
  bool ok=readOpen(completeText)==Token::Valid;
  if(ok)ok=io_.rewind();
  const auto n=std::strlen(completeText);
  if(ok)ok=io_.write(completeText,n)==static_cast<int>(n);
  if(ok)ok=io_.sync();
  return finish(ok);
 }
 bool faulted()const{return fault_;}
};
}
