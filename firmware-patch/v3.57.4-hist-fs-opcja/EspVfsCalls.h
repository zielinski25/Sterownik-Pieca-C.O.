#pragma once
#include "Storage.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace history_a191 {
// Candidate PUBLIC VFS bridge: never extracts private lfs_t, never mounts,
// formats, or retries close. The caller MUST confirm the mounted backend is
// the pinned esp_littlefs and its base path is /littlefs. Wired only in offline candidate195; not ESP-qualified.
class EspVfsCalls final:public Calls {
 int fd_=-1;bool uncertain_=false,ioError_=false;
 static const char*name(Path p){
  switch(p){
   case Path::Request:return "/littlefs/history_reset_a1.req";
   case Path::Completion:return "/littlefs/history_reset_a1.done";
   case Path::Short:return "/littlefs/hist_short.csv";
   case Path::Long:return "/littlefs/hist_long.csv";
  }return nullptr;
 }
public:
 EspVfsCalls()=default;
 EspVfsCalls(const EspVfsCalls&)=delete;
 EspVfsCalls&operator=(const EspVfsCalls&)=delete;
 // No destructor retry. Every successful open must get exactly one close.
 int stat(Path p)override{
  if(uncertain_||ioError_)return -1;
  errno=0;struct ::stat s{};
  if(::stat(name(p),&s)!=0){if(errno==ENOENT)return -2;ioError_=true;return -1;}
  // Optional mtime can be absent on legacy files (ENODATA), but an I/O error
  // in the wrapper's ignored mtime lookup must stop the operation.
  if(errno!=0&&errno!=ENODATA){ioError_=true;return -1;}
  if(!S_ISREG(s.st_mode)){ioError_=true;return -1;}return 0;
 }
 int open(Path p,Open mode)override{
  if(uncertain_||ioError_||fd_>=0)return -1;
  int flags=mode==Open::Read?O_RDONLY:mode==Open::ExistingReadWrite?O_RDWR:O_RDWR|O_CREAT|O_EXCL;
  errno=0;
  fd_=::open(name(p),flags,0600);
  if(fd_<0){if(errno==ENOENT)return -2;ioError_=true;return -1;}
  // Pinned esp_littlefs can return an FD after its initial sync failed,
  // reporting the error only in errno. Conservative source-specific check:
  // never infer a successful initial sync from a nonnegative FD alone.
  if(errno!=0){const int old=fd_;fd_=-1;::close(old);uncertain_=true;return -1;}
  return 0;
 }
 int read(void*p,std::size_t n)override{if(fd_<0||quarantined())return -1;errno=0;int r=::read(fd_,p,n);if(r<0||errno!=0){ioError_=true;return -1;}return r;}
 int write(const void*p,std::size_t n)override{
  if(fd_<0||quarantined())return -1;
  errno=0;int r=::write(fd_,p,n);
  // FLUSH_FILE_EVERY_WRITE may hide its failed sync behind a positive count.
  if(r!=static_cast<int>(n)||errno!=0){ioError_=true;return -1;}return r;
 }
 bool rewind()override{if(fd_<0||quarantined())return false;errno=0;if(::lseek(fd_,0,SEEK_SET)!=0||errno!=0){ioError_=true;return false;}return true;}
 bool sync()override{if(fd_<0||quarantined())return false;errno=0;if(::fsync(fd_)!=0||errno!=0){ioError_=true;return false;}return true;}
 bool close()override{
  if(fd_<0||uncertain_)return false;
  const int old=fd_;fd_=-1;
  errno=0;if(::close(old)!=0||errno!=0||ioError_){uncertain_=true;return false;}return true;
 }
 bool remove(Path p)override{if(quarantined())return false;errno=0;if(::unlink(name(p))!=0||errno!=0){ioError_=true;return false;}return true;}
 bool quarantined()const{return uncertain_||ioError_;}
};
}
