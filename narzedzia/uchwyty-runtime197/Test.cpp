#include "RuntimeFilesystem.h"
#include "CheckedRead.h"
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <cassert>
#include <iostream>
#include <cstdlib>
extern std::atomic<unsigned long> allocations;
struct Lock:history197::Mutex{std::mutex m;std::atomic<unsigned> waiting{0};void lock()override{++waiting;m.lock();--waiting;}void unlock()override{m.unlock();}};
struct Model {
 std::map<std::string,std::string>files;std::vector<std::string>events;std::function<void(const char*)>hook;
 unsigned closes=0,destroyed=0;bool partial=false,invalidReadCount=false;std::vector<std::string>children;
 Model(){events.reserve(10000);files["/a"]="OLD";files["/b"]="KEEP";files["/dir"]="";children={"/a","/b"};}
 void hit(const char*p){events.push_back(p);if(hook)hook(p);}
};
struct BackendFile:fs::FileImpl {
 Model&m;std::string p;size_t pos=0,child=0;bool live=true;
 BackendFile(Model&x,const char*s):m(x),p(s){}
 ~BackendFile(){++m.destroyed;if(live)close();}
 size_t write(const uint8_t*b,size_t n)override{m.hit("write");if(m.partial)n=n?n-1:0;auto&s=m.files[p];if(pos+n>s.size())s.resize(pos+n);if(n)std::memcpy(&s[pos],b,n);pos+=n;return n;}
 size_t read(uint8_t*b,size_t n)override{m.hit("read");auto&s=m.files[p];n=std::min(n,s.size()-std::min(pos,s.size()));if(n)std::memcpy(b,s.data()+pos,n);pos+=n;return m.invalidReadCount?n+1:n;}
 void flush()override{m.hit("flush");}bool seek(uint32_t n,fs::SeekMode)override{m.hit("seek");pos=n;return true;}
 size_t position()const override{m.hit("position");return pos;}size_t size()const override{m.hit("size");return m.files[p].size();}
 bool setBufferSize(size_t)override{m.hit("buffer");return true;}void close()override{if(live){m.hit("close");++m.closes;live=false;}}
 time_t getLastWrite()override{m.hit("mtime");return 123;}const char*path()const override{m.hit("path");return p.c_str();}const char*name()const override{m.hit("name");return p.c_str()+1;}
 bool isDirectory()override{m.hit("isdir");return p=="/dir";}
 fs::FileImplPtr openNextFile(const char*)override{m.hit("next");if(child==m.children.size())return {};return std::make_shared<BackendFile>(m,m.children[child++].c_str());}
 bool seekDir(long n)override{m.hit("seekdir");child=n;return true;}String getNextFileName()override{m.hit("nextname");return "entry";}String getNextFileName(bool*d)override{m.hit("nexttype");*d=false;return "entry";}void rewindDirectory()override{m.hit("rewind");child=0;}operator bool()override{return live;}
};
struct BackendImpl:fs::FSImpl {
 Model&m;explicit BackendImpl(Model&x):m(x){}
 fs::FileImplPtr open(const char*p,const char*mode,bool create)override{m.hit("open");if(!m.files.count(p)){if(!create&&mode[0]=='r')return {};m.files[p]="";}if(mode[0]=='w')m.files[p]="";return std::make_shared<BackendFile>(m,p);}
 bool exists(const char*p)override{m.hit("exists");return m.files.count(p);}bool remove(const char*p)override{m.hit("remove");return m.files.erase(p);}
 bool rename(const char*a,const char*b)override{m.hit("rename");if(!m.files.count(a))return false;m.files[b]=m.files[a];m.files.erase(a);return true;}
 bool mkdir(const char*p)override{m.hit("mkdir");m.files[p]="";return true;}bool rmdir(const char*p)override{m.hit("rmdir");return m.files.erase(p);}
};
struct Backend:fs::FS {Model&m;explicit Backend(Model&x):fs::FS(std::make_shared<BackendImpl>(x)),m(x){}size_t totalBytes(){m.hit("total");return 10000;}size_t usedBytes(){m.hit("used");return 123;}};
using Owner=history197::Owner<Backend,4>;
// Process-lifetime test fixtures: quarantined files are intentionally NOT
// destroyed. All memory remains reachable; LSan is not disabled.
struct Fixture {Model model;Lock lock;Backend backend;history197::Permanent<Owner> storage;Owner&o;Fixture():backend(model),o(storage.construct(backend,lock)){} };
static Fixture*fixtures[256]{};static unsigned fixtureCount=0;
static Fixture&fresh(bool enabled=true){assert(fixtureCount<256);auto&f=*(fixtures[fixtureCount++]=new Fixture);if(enabled)assert(f.o.enable());return f;}
static void denied(Fixture&f,fs::File&file){auto n=f.model.events.size();auto close=f.model.closes;auto destroyed=f.model.destroyed;uint8_t b[8]{};bool directory=true;
 fs::FS copy=f.o.fs();assert(!copy.open("/a","w")&&!copy.exists("/a")&&!copy.remove("/a")&&!copy.rename("/a","/b")&&!copy.mkdir("/x")&&!copy.rmdir("/dir"));assert(f.o.totalBytes()==0&&f.o.usedBytes()==0);
 assert(!file&&file.write(b,1)==0&&file.read()==-1);assert(file.read(b,8)==size_t(-1)); // ACTUAL FS.cpp sentinel, transport must validate!
 assert(file.size()==0&&file.position()==size_t(-1)&&!file.seek(0)&&!file.setBufferSize(1));assert(!file.path()&&!file.name()&&!file.isDirectory()&&!file.openNextFile());file.flush();file.rewindDirectory();assert(!file.seekDir(0));file.getNextFileName();file.getNextFileName(&directory);assert(file.getLastWrite()==0);file.close();
 assert(f.model.events.size()==n&&f.model.closes==close&&f.model.destroyed==destroyed);
}
class AsyncFileResponse {fs::File _content;public:explicit AsyncFileResponse(const fs::File&f):_content(f){}size_t _fillBuffer(uint8_t*,size_t);};
#include "WebFill.inc"
int main(){unsigned groups=0;
 {auto&f=fresh(false);fs::File empty;denied(f,empty);assert(f.o.enable());assert(!f.o.enable());f.o.trip();assert(!f.o.enable()&&f.o.drained());denied(f,empty);++groups;}
 {auto&f=fresh();auto a=f.o.fs().open("/a");auto b=a;assert(a&&b);auto d=f.model.destroyed; a.close();assert(!b&&f.model.closes==1&&f.model.destroyed==d+1);b.close();assert(f.model.closes==1);for(int k=0;k<1000;++k){auto x=f.o.fs().open("/a");assert(x);}assert(f.model.closes==1001);++groups;}
 {auto&f=fresh();fs::File a[4];for(auto&x:a){x=f.o.fs().open("/a");assert(x);}auto calls=f.model.events.size();assert(!f.o.fs().open("/a","w")&&f.model.events.size()==calls);auto alias=a[0];a[0].close();assert(!f.o.fs().open("/a","w"));alias.close();auto again=f.o.fs().open("/a");assert(again);++groups;}
 {auto&f=fresh();auto a=f.o.fs().open("/a");auto b=a;f.o.trip();assert(f.o.drained());auto n=f.model.events.size();a=fs::File();denied(f,b);assert(f.model.events.size()==n&&f.model.closes==0&&f.model.destroyed==0&&f.o.retained()==1);++groups;}
 for(const char*point:{"open","close","write","read","flush","next","path"}){auto&f=fresh();fs::File a;
  if(std::strcmp(point,"open"))a=f.o.fs().open(std::strcmp(point,"next")&&std::strcmp(point,"path")?"/a":"/dir");
  f.model.hook=[&](const char*event){if(!std::strcmp(event,point))f.o.trip();};uint8_t byte=65;
  if(!std::strcmp(point,"open"))a=f.o.fs().open("/a");else if(!std::strcmp(point,"close"))a.close();else if(!std::strcmp(point,"write"))assert(a.write(&byte,1)==0);else if(!std::strcmp(point,"read"))a.read(&byte,1);else if(!std::strcmp(point,"flush"))a.flush();else{auto child=a.openNextFile();assert(!child);}
  assert(!f.o.ready()&&f.o.drained()&&f.model.destroyed==0);f.model.hook={};auto n=f.model.events.size();auto closes=f.model.closes,dead=f.model.destroyed;denied(f,a);a=fs::File();assert(f.model.events.size()==n&&f.model.closes==closes&&f.model.destroyed==dead&&f.o.retained()>0);++groups;
 }
 {auto&f=fresh();auto dir=f.o.fs().open("/dir");auto a=dir.openNextFile();auto b=dir.openNextFile();assert(a&&b&&!dir.openNextFile());assert(std::string(a.path())=="/a"&&std::string(a.name())=="a");auto alias=a;f.o.trip();assert(f.o.drained());denied(f,a);denied(f,b);denied(f,dir);denied(f,alias);assert(f.o.retained()==3&&f.model.closes==0);++groups;}
 {auto&f=fresh();auto a=f.o.fs().open("/a");uint8_t b[3]={1,2,3};f.model.partial=true;assert(a.write(b,3)==0&&!f.o.ready()&&f.o.fault()==history197::Fault::ShortWrite);f.o.trip();assert(f.o.fault()==history197::Fault::ShortWrite);denied(f,a);++groups;}
 {auto&f=fresh();std::string longpath(128,'x');auto n=f.model.events.size();assert(!f.o.fs().open(longpath.c_str())&&f.model.events.size()==n);auto a=f.o.fs().open("/missing");assert(!a&&f.o.ready());for(int k=0;k<20;++k){auto b=f.o.fs().open("/a");assert(b);}++groups;}
 {auto&f=fresh();auto before=allocations.load();auto a=f.o.fs().open("/a");auto delta=allocations.load()-before;assert(a&&delta==1); // only modeled BACKEND FileImpl; wrapper uses fixed arena
  a.close();++groups;}
 // Positive controls for forwarding, metadata, directories and checked EOF.
 {auto&f=fresh();auto a=f.o.fs().open("/a");uint8_t b[8]{};auto first=history197::checkedRead(f.o,a,b,3);assert(first.status==history197::ReadStatus::Data&&first.bytes==3&&!std::memcmp(b,"OLD",3));assert(a.position()==3&&a.size()==3);assert(history197::checkedRead(f.o,a,b,3).status==history197::ReadStatus::End);assert(a.seek(0)&&a.peek()=='O'&&a.available()==3&&a.setBufferSize(128)&&a.getLastWrite()==123);a.flush();assert(std::string(a.path())=="/a"&&!a.isDirectory());assert(f.o.totalBytes()==10000&&f.o.usedBytes()==123);assert(f.o.fs().mkdir("/x")&&f.o.fs().exists("/x")&&f.o.fs().rename("/x","/y")&&f.o.fs().remove("/y"));auto dir=f.o.fs().open("/dir");bool isdir=true;assert(dir.isDirectory()&&dir.seekDir(0));dir.getNextFileName();dir.getNextFileName(&isdir);dir.rewindDirectory();assert(f.o.fs().rmdir("/dir"));++groups;}
 {auto&f=fresh();auto a=f.o.fs().open("/a");uint8_t b[2]{};auto n=f.model.events.size();assert(history197::checkedRead(f.o,a,b,3).status==history197::ReadStatus::Invalid&&f.model.events.size()==n);f.model.invalidReadCount=true;auto bad=history197::checkedRead(f.o,a,b,2);assert(bad.status==history197::ReadStatus::Stopped&&bad.bytes==0&&f.o.fault()==history197::Fault::ReadCount);denied(f,a);++groups;}
 {auto&f=fresh();auto a=f.o.fs().open("/a");uint8_t b[8]{};f.o.trip();auto n=f.model.events.size();assert(history197::checkedRead(f.o,a,b,8).status==history197::ReadStatus::Stopped&&f.model.events.size()==n);++groups;}
 // Concurrent healthy reuse: all 4 slots, no raw backend access in parallel.
 {auto&f=fresh();std::thread workers[4];for(auto&t:workers)t=std::thread([&]{for(int k=0;k<250;++k){auto a=f.o.fs().open("/a");assert(a&&a.size()==3);a.close();}});for(auto&t:workers)t.join();assert(f.model.closes==1000&&f.o.ready());++groups;}
 // No directory advance when pool is full; no child close after an invalid path.
 {auto&f=fresh();auto dir=f.o.fs().open("/dir");fs::File a[3];for(auto&x:a)x=f.o.fs().open("/a");auto n=f.model.events.size();assert(!dir.openNextFile()&&f.model.events.size()==n);a[0].close();auto first=dir.openNextFile();assert(first&&std::string(first.path())=="/a");++groups;}
 {auto&f=fresh();f.model.children={std::string(128,'x')};auto dir=f.o.fs().open("/dir");auto child=dir.openNextFile();assert(!child&&!f.o.ready()&&f.o.fault()==history197::Fault::Path&&f.model.closes==0&&f.model.destroyed==0&&f.o.retained()==2);denied(f,dir);++groups;}
 for(const char*point:{"exists","remove","rename","mkdir","rmdir","total","used"}){auto&f=fresh();auto a=f.o.fs().open("/a");f.model.hook=[&](const char*event){if(!std::strcmp(event,point))f.o.trip();};
  if(!std::strcmp(point,"exists"))assert(!f.o.fs().exists("/b"));
  if(!std::strcmp(point,"remove"))assert(!f.o.fs().remove("/b"));
  if(!std::strcmp(point,"rename"))assert(!f.o.fs().rename("/b","/c"));
  if(!std::strcmp(point,"mkdir"))assert(!f.o.fs().mkdir("/x"));
  if(!std::strcmp(point,"rmdir"))assert(!f.o.fs().rmdir("/dir"));
  if(!std::strcmp(point,"total"))assert(f.o.totalBytes()==0);
  if(!std::strcmp(point,"used"))assert(f.o.usedBytes()==0);
  assert(!f.o.ready()&&f.o.drained());f.model.hook={};denied(f,a);++groups;}
 // A weak shared_ptr can outlive the proxy object: its control block MUST
 // keep its arena unavailable until the last weak reference is released.
 {auto&f=fresh();struct Inspect:fs::File{explicit Inspect(const fs::File&x):fs::File(x){}std::weak_ptr<fs::FileImpl> weak(){return _p;}};
  auto a=f.o.fs().open("/a");Inspect observer(a);auto weak=observer.weak();a.close();observer.close();assert(weak.expired());fs::File others[3];for(auto&x:others)x=f.o.fs().open("/a");auto n=f.model.events.size();assert(!f.o.fs().open("/a")&&f.model.events.size()==n);weak.reset();auto again=f.o.fs().open("/a");assert(again);++groups;}
 // Actual pinned file-response fill method forwards the Arduino sentinel.
 {auto&f=fresh();auto a=f.o.fs().open("/a");AsyncFileResponse response(a);uint8_t b[8]{};f.o.trip();size_t result=response._fillBuffer(b,8);assert(result==size_t(-1));assert(static_cast<uint32_t>(result)==tryAgain197);if(sizeof(size_t)>4)assert(result!=tryAgain197);assert(history197::checkedRead(f.o,a,b,8).status==history197::ReadStatus::Stopped);++groups;}
 // Admitted write may finish. Waiting write and destructor cannot do I/O after drain.
 {auto&f=fresh();auto a=f.o.fs().open("/a");auto b=a;std::mutex m;std::condition_variable cv;bool entered=false,release=false;size_t first=99,second=99;
  f.model.hook=[&](const char*op){if(std::strcmp(op,"write"))return;std::unique_lock<std::mutex>l(m);entered=true;cv.notify_all();cv.wait(l,[&]{return release;});};
  std::thread t([&]{uint8_t x=88;first=a.write(&x,1);});{std::unique_lock<std::mutex>l(m);cv.wait(l,[&]{return entered;});}
  f.o.trip();std::atomic<bool> drainedDone{false};std::thread drainer([&]{assert(f.o.drained());drainedDone=true;});while(!f.lock.waiting.load()&&!drainedDone.load())std::this_thread::yield();assert(!drainedDone.load()&&f.lock.waiting.load());std::thread u([&]{uint8_t x=89;second=b.write(&x,1);});{std::lock_guard<std::mutex>l(m);release=true;}cv.notify_all();t.join();u.join();drainer.join();assert(drainedDone&&first==0&&second==0&&f.o.drained());f.model.hook={};assert(f.model.files["/a"]=="XLD");auto n=f.model.events.size();denied(f,a);denied(f,b);assert(f.model.events.size()==n);++groups;}
 // The same lease can serialize the future checked public-FD path. No proxy calls inside it.
 {auto&f=fresh();{Owner::Lease lease(f.o);assert(lease);f.model.hit("raw-admitted");f.o.trip();}assert(f.o.drained());Owner::Lease refused(f.o);assert(!refused);++groups;}
 std::cout<<"PASS runtime197 groups="<<groups<<" bounded slots=4; 1000 reuse cycles; real FS.cpp; explicit post-stop read sentinel; not integrated firmware\n";
 std::cout<<"host sizeof Owner4="<<sizeof(Owner)<<" Owner10="<<sizeof(history197::Owner<Backend>)<<"; not ESP ABI\n";
}
