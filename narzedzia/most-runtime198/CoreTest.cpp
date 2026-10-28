#include "RuntimeHistory.h"
#include "Model197.inc"
#include "lfs.h"
#include <array>
// A new blank RAM medium in each fixture. NEVER formats or mounts a device.
struct Media {
 std::array<unsigned char,65536>image; lfs_config cfg{};lfs_t fs{};
 unsigned calls=0,target=0;bool armed=false,hit=false;
 Media(){image.fill(255);cfg.context=this;cfg.read=read;cfg.prog=prog;cfg.erase=erase;cfg.sync=sync;cfg.read_size=16;cfg.prog_size=16;cfg.block_size=512;cfg.block_count=128;cfg.block_cycles=100;cfg.cache_size=64;cfg.lookahead_size=16;assert(lfs_format(&fs,&cfg)==0);assert(lfs_mount(&fs,&cfg)==0);}
 static Media&get(const lfs_config*c){return *static_cast<Media*>(c->context);}
 bool fail(){if(!armed)return false;++calls;if(calls==target){hit=true;return true;}return false;}
 static int read(const lfs_config*c,lfs_block_t b,lfs_off_t o,void*p,lfs_size_t n){auto&m=get(c);assert(b<128&&o+n<=512);if(m.fail())return LFS_ERR_IO;std::memcpy(p,m.image.data()+b*512+o,n);return 0;}
 static int prog(const lfs_config*c,lfs_block_t b,lfs_off_t o,const void*p,lfs_size_t n){auto&m=get(c);assert(b<128&&o+n<=512);if(m.fail())return LFS_ERR_IO;auto*s=static_cast<const unsigned char*>(p);for(unsigned i=0;i<n;++i){auto&dst=m.image[b*512+o+i];assert((dst&s[i])==s[i]);dst&=s[i];}return 0;}
 static int erase(const lfs_config*c,lfs_block_t b){auto&m=get(c);assert(b<128);if(m.fail())return LFS_ERR_IO;std::memset(m.image.data()+b*512,255,512);return 0;}
 static int sync(const lfs_config*c){return get(c).fail()?LFS_ERR_IO:0;}
 void put(const char*p,const std::string&s){lfs_file_t f{};assert(lfs_file_open(&fs,&f,p,LFS_O_RDWR|LFS_O_CREAT|LFS_O_EXCL)==0);assert(lfs_file_write(&fs,&f,s.data(),s.size())==static_cast<int>(s.size()));assert(lfs_file_close(&fs,&f)==0);}
 std::string text(const char*p){assert(!armed);lfs_file_t f{};assert(lfs_file_open(&fs,&f,p,LFS_O_RDONLY)==0);int n=lfs_file_size(&fs,&f);assert(n>=0);std::string s(n,'\0');assert(lfs_file_read(&fs,&f,n?&s[0]:nullptr,n)==n);assert(lfs_file_close(&fs,&f)==0);return s;}
};
// Direct core adapter, NOT esp_littlefs VFS. Arduino side remains the 197 model.
struct CoreCalls:history193::Calls {
 Media&m;std::map<int,lfs_file_t>fds;int next=0,err=0;unsigned calls=0,closes=0;
 explicit CoreCalls(Media&x):m(x){}
 int result(int n){err=n<0?EIO:0;return n;}
 int open(const char*p,int flags)override{++calls;assert(std::strncmp(p,"/littlefs/",10)==0);int id=++next;int mode=(flags&O_RDWR)?LFS_O_RDWR:LFS_O_RDONLY;if(flags&O_CREAT)mode|=LFS_O_CREAT;if(flags&O_EXCL)mode|=LFS_O_EXCL;int r=result(lfs_file_open(&m.fs,&fds[id],p+9,mode));if(r<0){fds.erase(id);return -1;}return id;}
 int stat(int fd,struct stat*s)override{++calls;int n=result(lfs_file_size(&m.fs,&fds.at(fd)));if(n<0)return -1;std::memset(s,0,sizeof(*s));s->st_mode=S_IFREG;s->st_size=n;return 0;}
 off_t seek(int fd,off_t p)override{++calls;return result(lfs_file_seek(&m.fs,&fds.at(fd),p,LFS_SEEK_SET));}
 ssize_t read(int fd,void*p,size_t n)override{++calls;return result(lfs_file_read(&m.fs,&fds.at(fd),p,n));}
 ssize_t write(int fd,const void*p,size_t n)override{++calls;return result(lfs_file_write(&m.fs,&fds.at(fd),p,n));}
 int sync(int fd)override{++calls;return result(lfs_file_sync(&m.fs,&fds.at(fd)));}
 int close(int fd)override{++calls;++closes;int r=result(lfs_file_close(&m.fs,&fds.at(fd)));if(r>=0)fds.erase(fd);return r;}
 int rename(const char*a,const char*b)override{++calls;return result(lfs_rename(&m.fs,a+9,b+9));}
 int error()const override{return err;}
};
static std::string old(){return std::string(300,'a')+"\n"+std::string(300,'b')+"\n"+std::string(300,'c')+"\n";}
struct Fixture {
 Media media;Model model;Lock lock;Backend backend;history197::Permanent<Owner>os;Owner&o;CoreCalls raw;
 using History=history198::RuntimeHistory<Owner>;history197::Permanent<History>hs;History&h;
 Fixture():backend(model),o(os.construct(backend,lock)),raw(media),h(hs.construct(o,raw)){media.put("/hist_short.csv",old());media.put("/settings","KEEP");assert(o.enable());}
};
static Fixture*roots[4096]{};static unsigned used=0;
static Fixture&fresh(){assert(used<4096);return *(roots[used++]=new Fixture);}
int main(){unsigned faults=0,stopped=0,recovered=0;
 for(bool trim:{false,true}){auto&base=fresh();base.media.armed=true;uint64_t n=0;unsigned lines=0;std::string data=std::string(600,'x')+"\n";
  auto run=[&](Fixture&f){return trim?f.h.trim(history193::Stream::Short,1,n,lines):f.h.append(history193::Stream::Short,data.data(),data.size(),n);};
  assert(run(base).confirmed);unsigned count=base.media.calls;base.media.armed=false;assert(base.media.text("/hist_short.csv")== (trim?std::string(300,'c')+"\n":old()+data));assert(base.media.text("/settings")=="KEEP");
  for(unsigned at=1;at<=count;++at){auto&f=fresh();auto file=f.o.fs().open("/a");f.media.target=at;f.media.armed=true;auto r=run(f);assert(f.media.hit);
   if(!r.confirmed){++stopped;assert(!r.ready&&f.o.drained());auto calls=f.raw.calls,mediaCalls=f.media.calls,closes=f.raw.closes;assert(!f.h.append(history193::Stream::Short,"no\n",3,n).admitted);assert(!f.h.trim(history193::Stream::Short,1,n,lines).admitted);assert(!file);file.close();assert(f.raw.calls==calls&&f.media.calls==mediaCalls&&f.raw.closes==closes&&f.model.closes==0);}
   else{++recovered;assert(r.ready&&f.raw.fds.empty());}
   // No read/close/unmount/remount on uncertain live state, even for inspection.
   ++faults;
  }
 }
 std::cout<<"PASS pinned LittleFS core runtime198 media-faults="<<faults<<" stopped="<<stopped<<" recovered="<<recovered<<"; immediate RAM medium; no uncertain-state cleanup; not ESP VFS\n";
}
