#include "RuntimeHistory.h"
#include "Model197.inc"
#include <chrono>
#include <algorithm>
using HistoryStream=history193::Stream;using history193::Fault;
static const char*shortPath="/hist_short.csv";
struct Raw:history193::Calls {
 enum Mode{Healthy,Negative,PositiveError,Stop,ShortCount,LargeCount,WrongSeek};
 struct Handle {std::string path;size_t pos=0;};
 Model&m;Owner&o;std::map<int,Handle>fds;std::vector<std::string>trace;
 size_t calls=0,target=0;int next=10,err=0,injectedError=EIO;Mode mode=Healthy;
 bool consumeClose=true,enodata=false,badType=false,badSize=false,skewSize=false,corrupt=false,written=false;
 explicit Raw(Model&x,Owner&owner):m(x),o(owner){}
 Mode tick(const char*op){err=0;++calls;trace.push_back(op);std::string event=std::string("fd.")+op;m.hit(event.c_str());if(calls!=target)return Healthy;if(mode==Stop)o.trip();if(mode==Negative||mode==PositiveError)err=injectedError;return mode;}
 static std::string path(const char*p){assert(std::strncmp(p,"/littlefs/",10)==0);return p+9;}
 int open(const char*p,int flags)override{
  auto mode=tick("open");if(mode==Negative)return -1;assert(!(flags&O_TRUNC));auto n=path(p);
  if((flags&O_EXCL)&&m.files.count(n)){err=EEXIST;return -1;}
  if(!(flags&O_CREAT)&&!m.files.count(n)){err=ENOENT;return -1;}
  m.files[n];int fd=next++;fds[fd].path=n;return fd;
 }
 int stat(int fd,struct stat*s)override{auto mode=tick("stat");if(mode==Negative)return -1;std::memset(s,0,sizeof(*s));s->st_mode=badType?S_IFDIR:S_IFREG;s->st_size=badSize?-1:static_cast<off_t>(m.files.at(fds.at(fd).path).size())+(skewSize&&written?1:0);if(enodata)err=ENODATA;return 0;}
 off_t seek(int fd,off_t pos)override{auto mode=tick("seek");if(mode==Negative)return -1;assert(pos>=0);fds.at(fd).pos=pos;return mode==WrongSeek?pos+1:pos;}
 ssize_t read(int fd,void*p,size_t n)override{auto mode=tick("read");if(mode==Negative)return -1;auto&h=fds.at(fd);auto&s=m.files.at(h.path);auto got=std::min(n,s.size()-std::min(s.size(),h.pos));if(got)std::memcpy(p,s.data()+h.pos,got);h.pos+=got;if(corrupt&&written&&got)static_cast<char*>(p)[0]^=1;return mode==ShortCount?static_cast<ssize_t>(got)-1:mode==LargeCount?got+1:got;}
 ssize_t write(int fd,const void*p,size_t n)override{auto mode=tick("write");if(mode==Negative)return -1;auto&h=fds.at(fd);auto&s=m.files.at(h.path);size_t got=mode==ShortCount?n-1:n;if(h.pos+got>s.size())s.resize(h.pos+got);if(got)std::memcpy(&s[h.pos],p,got);h.pos+=got;written=true;return mode==LargeCount?n+1:got;}
 int sync(int fd)override{assert(fds.count(fd));return tick("sync")==Negative?-1:0;}
 int close(int fd)override{assert(fds.count(fd));auto mode=tick("close");if(mode!=Negative||consumeClose)fds.erase(fd);return mode==Negative?-1:0;}
 int rename(const char*a,const char*b)override{assert(fds.empty());if(tick("rename")==Negative)return -1;auto src=path(a),dst=path(b);m.files[dst]=m.files.at(src);m.files.erase(src);return 0;}
 int error()const override{return err;}
};
template<size_t C=2>struct Fixture {
 Model model;Lock lock;Backend backend;history197::Permanent<Owner>os;Owner&o;Raw raw;
 unsigned notifications=0;bool observerSawStop=false;
 using History=history198::RuntimeHistory<Owner,C>;
 history197::Permanent<History>hs;History&h;
 static void observed(void*p){auto&f=*static_cast<Fixture*>(p);++f.notifications;f.observerSawStop=!f.o.ready();}
 Fixture():backend(model),o(os.construct(backend,lock)),raw(model,o),h(hs.construct(o,raw,nullptr,observed,this)){model.files[shortPath]="old\n";model.files["/hist_long.csv"]="long\n";}
};
static void*roots[4096]{};static size_t rootCount=0;
template<size_t C=2>static Fixture<C>&fresh(bool ready=true){assert(rootCount<4096);auto*p=new Fixture<C>;roots[rootCount++]=p;if(ready)assert(p->o.enable());return *p;}
static std::string payload(){return std::string(600,'x')+"\n";}
static std::string rows(){return std::string(300,'a')+"\n"+std::string(300,'b')+"\n"+std::string(300,'c')+"\n";}
static void denied(Fixture<>&f,fs::File&file){assert(!f.o.ready()&&f.o.drained());auto calls=f.raw.calls,events=f.model.events.size();uint64_t size=99;unsigned lines=88;
 auto r=f.h.append(HistoryStream::Short,"no\n",3,size);assert(!r.admitted&&!r.confirmed&&!r.ready&&size==99);
 r=f.h.trim(HistoryStream::Short,1,size,lines);assert(!r.admitted&&!r.confirmed&&size==99&&lines==88);
 fs::FS copy=f.o.fs();assert(!copy.open("/a","w")&&!copy.exists("/a")&&!copy.remove("/a"));uint8_t b[4]{};assert(file.write(b,1)==0&&file.read(b,4)==size_t(-1));file.close();
 assert(f.raw.calls==calls&&f.model.events.size()==events&&!f.o.enable());
}
static void waitFor(const std::function<bool()>&condition){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!condition()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}}
struct LateOwner {
 Owner&native;mutable unsigned checks=0;
 explicit LateOwner(Owner&o):native(o){}
 struct Lease {Owner::Lease inner;explicit Lease(LateOwner&o):inner(o.native){}explicit operator bool()const{return static_cast<bool>(inner);}};
 bool ready()const{if(checks&&!--checks)native.trip();return native.ready();}
 void trip(){native.trip();}
};
struct LateFixture:Fixture<> {
 LateOwner late;using LateHistory=history198::RuntimeHistory<LateOwner>;
 history197::Permanent<LateHistory>ls;LateHistory&lh;
 LateFixture():late(o),lh(ls.construct(late,raw)){}
};
int main(){unsigned groups=0,injected=0;
 // Positive append, shared File read, trim and no-op trim; bounded slots reused.
 {auto&f=fresh();uint64_t size=0;unsigned lines=0;auto data=payload();auto r=f.h.append(HistoryStream::Short,data.data(),data.size(),size);assert(r.admitted&&r.confirmed&&r.ready&&size==data.size()+4&&r.unresolved==0&&f.raw.fds.empty());auto file=f.o.fs().open(shortPath);assert(file.size()==size);assert(file.seek(4));uint8_t b[8]{};assert(file.read(b,8)==8&&std::string(reinterpret_cast<char*>(b),8)==std::string(8,'x'));file.close();r=f.h.trim(HistoryStream::Short,1,size,lines);assert(r.confirmed&&size==data.size()&&lines==1&&f.model.files[shortPath]==data);r=f.h.trim(HistoryStream::Short,9,size,lines);assert(r.confirmed&&lines==1);assert(!f.notifications);++groups;}
 {auto&f=fresh();uint64_t size=0;for(unsigned i=0;i<1000;++i){auto r=f.h.append(HistoryStream::Long,"a\n",2,size);assert(r.confirmed&&r.unresolved==0);}assert(size==2005&&f.raw.fds.empty());++groups;}
 // Cold rejection does not touch either storage path or irreversibly poison startup.
 {auto&f=fresh(false);uint64_t size=7;auto r=f.h.append(HistoryStream::Short,"x\n",2,size);assert(!r.admitted&&!r.confirmed&&size==7&&f.raw.calls==0&&f.notifications==0);assert(f.o.enable());assert(f.h.append(HistoryStream::Short,"x\n",2,size).confirmed);++groups;}
 // Every raw call position, with negative error, positive+errno, and external stop.
 for(bool trim:{false,true}){
  auto&base=fresh();base.model.files[shortPath]=rows();uint64_t n=0;unsigned lines=0;auto data=payload();
  assert((trim?base.h.trim(HistoryStream::Short,1,n,lines):base.h.append(HistoryStream::Short,data.data(),data.size(),n)).confirmed);
  const auto count=base.raw.calls;
  for(size_t at=1;at<=count;++at)for(auto mode:{Raw::Negative,Raw::PositiveError,Raw::Stop}){
   auto&f=fresh();f.model.files[shortPath]=rows();auto file=f.o.fs().open("/a");auto alias=file;
   f.raw.target=at;f.raw.mode=mode;uint64_t out=12345;unsigned outLines=99;
   auto r=trim?f.h.trim(HistoryStream::Short,1,out,outLines):f.h.append(HistoryStream::Short,data.data(),data.size(),out);
   assert(r.admitted&&!r.confirmed&&!r.ready&&r.fault!=Fault::None&&out==12345&&outLines==99);
   assert(f.raw.calls==at&&f.notifications==1&&f.observerSawStop);
   assert(r.unresolved==f.raw.fds.size()+(f.raw.trace.back()=="close"?1:0));
   assert(f.model.closes==0&&f.model.destroyed==0);denied(f,file);denied(f,alias);++injected;
  }++groups;
 }
 // Semantic failures must notify BEFORE Engine's cleanup requests.
 for(int which=0;which<5;++which){auto&f=fresh();auto file=f.o.fs().open("/a");if(which==0)f.model.files[shortPath]="no newline";if(which==1)f.raw.badType=true;if(which==2)f.raw.badSize=true;if(which==3)f.raw.corrupt=true;if(which==4)f.raw.skewSize=true;uint64_t n=9;auto r=f.h.append(HistoryStream::Short,"new\n",4,n);assert(!r.confirmed&&!r.ready&&r.unresolved==1&&f.notifications==1&&n==9);assert(std::count(f.raw.trace.begin(),f.raw.trace.end(),"close")==0);denied(f,file);++groups;}
 // Invalid input fails without any raw I/O, and cannot be cleared by later valid input.
 for(int which=0;which<6;++which){auto&f=fresh();auto file=f.o.fs().open("/a");std::string tooBig(4097,'x');tooBig.back()='\n';const char embedded[]={'x',0,'\n'};const char*p=which==0?nullptr:which==1?"":which==2?"bad":which==3?tooBig.data():embedded;size_t n=which==0?1:which==1?0:which==2?3:which==3?tooBig.size():3;uint64_t out=7;unsigned lines=9;auto r=which==5?f.h.trim(HistoryStream::Short,0,out,lines):f.h.append(HistoryStream::Short,p,n,out);assert(!r.confirmed&&r.fault==Fault::Input&&f.raw.calls==0&&f.notifications==1);denied(f,file);++groups;}
 // Unexpected counts, wrong seek, zero errno on a failed syscall, retained close.
 for(const char*op:{"read","write","seek","close","open"})for(int variant=0;variant<2;++variant){auto&base=fresh();uint64_t n=0;assert(base.h.append(HistoryStream::Short,"new\n",4,n).confirmed);auto at=std::find(base.raw.trace.begin(),base.raw.trace.end(),op)-base.raw.trace.begin()+1;auto&f=fresh();f.raw.target=at;std::string name=op;f.raw.mode=name=="seek"?Raw::WrongSeek:(name=="close"||name=="open")?Raw::Negative:variant?Raw::LargeCount:Raw::ShortCount;f.raw.consumeClose=false;if(variant)f.raw.injectedError=0;auto file=f.o.fs().open("/a");auto r=f.h.append(HistoryStream::Short,"new\n",4,n);assert(!r.confirmed&&!r.ready&&f.raw.calls==size_t(at));denied(f,file);++groups;}
 {auto&f=fresh();f.raw.enodata=true;uint64_t n=0;assert(f.h.append(HistoryStream::Short,"x\n",2,n).confirmed);assert(f.o.ready());++groups;}
 // Reservation exhaustion BEFORE the second raw open (no staging file created).
 {auto&f=fresh<1>();f.model.files[shortPath]=rows();uint64_t n=7;unsigned lines=8;auto r=f.h.trim(HistoryStream::Short,1,n,lines);assert(!r.confirmed&&!r.ready&&r.error==EMFILE&&r.unresolved==1&&n==7&&lines==8);assert(std::count(f.raw.trace.begin(),f.raw.trace.end(),"open")==1&&std::count(f.raw.trace.begin(),f.raw.trace.end(),"close")==0&&!f.model.files.count("/hist_short.csv.rebuild"));++groups;}
 // Existing staging evidence is never deleted or overwritten.
 {auto&f=fresh();f.model.files[shortPath]=rows();f.model.files["/hist_short.csv.rebuild"]="EVIDENCE";uint64_t n=0;unsigned lines=0;auto r=f.h.trim(HistoryStream::Short,1,n,lines);assert(!r.confirmed&&r.error==EEXIST&&f.model.files["/hist_short.csv.rebuild"]=="EVIDENCE"&&f.model.files[shortPath]==rows());++groups;}
 // Ordinary File fault denies the next history transaction without raw I/O.
 {auto&f=fresh();auto file=f.o.fs().open("/a");f.model.partial=true;uint8_t b=65;assert(file.write(&b,1)==0);denied(f,file);assert(f.raw.calls==0);++groups;}
 // Confirmed append remains confirmed if subsequent rotation fails.
 {auto&f=fresh();uint64_t n=0;unsigned lines=0;auto committed=f.h.append(HistoryStream::Short,"new\n",4,n);assert(committed.confirmed&&n==8);f.raw.target=f.raw.calls+1;f.raw.mode=Raw::Negative;auto trim=f.h.trim(HistoryStream::Short,1,n,lines);assert(!trim.confirmed&&!trim.ready&&committed.confirmed&&f.model.files[shortPath]=="old\nnew\n");++groups;}
 // Same Owner serializes raw-history transaction with real Arduino File methods.
 for(bool trimming:{false,true})for(bool stop:{false,true}){auto&f=fresh();if(trimming)f.model.files[shortPath]=rows();auto file=f.o.fs().open("/a");std::mutex m;std::condition_variable cv;bool entered=false,release=false;history198::Result result{};size_t fileResult=99;std::atomic<bool>drained{false};
  f.model.hook=[&](const char*op){if(std::strcmp(op,"fd.write"))return;std::unique_lock<std::mutex>l(m);entered=true;cv.notify_all();cv.wait(l,[&]{return release;});};
  std::thread history([&]{uint64_t n=0;unsigned lines=0;result=trimming?f.h.trim(HistoryStream::Short,1,n,lines):f.h.append(HistoryStream::Short,"new\n",4,n);});{std::unique_lock<std::mutex>l(m);cv.wait(l,[&]{return entered;});}
  std::thread other([&]{uint8_t b=65;fileResult=file.write(&b,1);});waitFor([&]{return f.lock.waiting.load()>=1;});
  std::thread drain;if(stop){f.o.trip();drain=std::thread([&]{assert(f.o.drained());drained=true;});waitFor([&]{return f.lock.waiting.load()>=2;});assert(!drained);}
  {std::lock_guard<std::mutex>l(m);release=true;cv.notify_all();}history.join();other.join();if(stop)drain.join();f.model.hook={};
  if(stop){assert(!result.confirmed&&fileResult==0&&drained&&f.model.files[shortPath]==(trimming?rows():"old\nnew\n"));assert(result.unresolved==(trimming?2:1));assert(std::count(f.raw.trace.begin(),f.raw.trace.end(),"close")==0);denied(f,file);}
  else{assert(result.confirmed&&fileResult==1);auto close=std::find(f.model.events.begin(),f.model.events.end(),trimming?"fd.rename":"fd.close"),write=std::find(f.model.events.begin(),f.model.events.end(),"write");assert(close<write);}++groups;
 }
 // Stop after the last checked close, before result.ready is sampled: preserve confirmation.
 {auto*p=new LateFixture;roots[rootCount++]=p;auto&f=*p;assert(f.o.enable());f.model.hook=[&](const char*op){if(!std::strcmp(op,"fd.close"))f.late.checks=2;};uint64_t n=0;auto r=f.lh.append(HistoryStream::Short,"new\n",4,n);assert(r.confirmed&&!r.ready&&n==8&&r.unresolved==0&&f.model.files[shortPath]=="old\nnew\n");f.model.hook={};++groups;}
 std::cout<<"PASS runtime198 groups="<<groups<<" injected-call-cases="<<injected<<" fixtures="<<rootCount<<"; real FS.cpp + extracted Engine; not ESP\n";
}
