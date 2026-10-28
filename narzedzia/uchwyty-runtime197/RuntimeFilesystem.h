#pragma once
#include <FS.h>
#include <FSImpl.h>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <new>
#include <utility>
namespace history197 {
struct Mutex {virtual ~Mutex(){};virtual void lock()=0;virtual void unlock()=0;};
enum class State { Cold, Ready, Stopped };
enum class Fault { None, External, ShortWrite, Path, ReadCount };
struct Block {
 alignas(std::max_align_t) unsigned char data[256];
 std::atomic<bool> busy{false};
};
template<class T> struct Allocator {
 using value_type=T;Block*b;
 explicit Allocator(Block*p):b(p){}
 template<class U> Allocator(const Allocator<U>&a):b(a.b){}
 template<class U>struct rebind{using other=Allocator<U>;};
 T*allocate(std::size_t n){static_assert(sizeof(T)<=256,"control block too large");static_assert(alignof(T)<=alignof(std::max_align_t),"control block alignment");if(n!=1||b->busy.exchange(true))std::abort();return reinterpret_cast<T*>(b->data);}
 void deallocate(T*,std::size_t){b->busy.store(false,std::memory_order_release);}
 template<class U>bool operator==(const Allocator<U>&a)const{return b==a.b;}
 template<class U>bool operator!=(const Allocator<U>&a)const{return b!=a.b;}
};
// MUST live until process reset. No destruction or reconstruction, including
// after stop: destroying the retained backend File objects could perform I/O.
// Backend and Mutex must outlive all proxy users. ESP port is not supplied.
template<class Backend,std::size_t Capacity=10,std::size_t PathBytes=128> class Owner {
 static_assert(Capacity>0&&PathBytes>1,"pool dimensions");
 enum SlotState { Free, Preparing, Live, Closed, Held };
 struct Slot {fs::File file,shadow;Block block;SlotState state=Free;char path[PathBytes]{};};
 Backend&backend_;Mutex&mutex_;std::atomic<State>state_{State::Cold};std::atomic<Fault>fault_{Fault::None};
 Slot slots_[Capacity];Block fsBlock_;fs::FS view_{fs::FSImplPtr()};
public:
 class Lease {
  Owner&o_;bool allowed_;
 public:explicit Lease(Owner&o):o_(o){o_.mutex_.lock();allowed_=o_.ready();}
  ~Lease(){o_.mutex_.unlock();}explicit operator bool()const{return allowed_;}
  Lease(const Lease&)=delete;Lease&operator=(const Lease&)=delete;
 };
private:
 class Node:public fs::FileImpl {
  Owner&o_;std::size_t i_;
 public:bool armed=false;Node(Owner&o,std::size_t i):o_(o),i_(i){}
  ~Node(){if(armed)o_.release(i_);}
  size_t write(const uint8_t*b,size_t n)override{return o_.write(i_,b,n);}
  size_t read(uint8_t*b,size_t n)override{return o_.read(i_,b,n);}
  void flush()override{o_.visit(i_,[](fs::File&f){f.flush();});}
  bool seek(uint32_t p,fs::SeekMode m)override{return o_.query<bool>(i_,false,[&](fs::File&f){return f.seek(p,m);});}
  size_t position()const override{return o_.query<size_t>(i_,size_t(-1),[](fs::File&f){return f.position();});}
  size_t size()const override{return o_.query<size_t>(i_,0,[](fs::File&f){return f.size();});}
  bool setBufferSize(size_t n)override{return o_.query<bool>(i_,false,[&](fs::File&f){return f.setBufferSize(n);});}
  void close()override{o_.release(i_);}
  time_t getLastWrite()override{return o_.query<time_t>(i_,0,[](fs::File&f){return f.getLastWrite();});}
  const char*path()const override{return o_.cachedPath(i_,false);}
  const char*name()const override{return o_.cachedPath(i_,true);}
  bool isDirectory()override{return o_.query<bool>(i_,false,[](fs::File&f){return f.isDirectory();});}
  fs::FileImplPtr openNextFile(const char*m)override{return o_.openNext(i_,m);}
  bool seekDir(long p)override{return o_.query<bool>(i_,false,[&](fs::File&f){return f.seekDir(p);});}
  String getNextFileName()override{return o_.query<String>(i_,String(),[](fs::File&f){return f.getNextFileName();});}
  String getNextFileName(bool*d)override{return o_.query<String>(i_,String(),[&](fs::File&f){return f.getNextFileName(d);});}
  void rewindDirectory()override{o_.visit(i_,[](fs::File&f){f.rewindDirectory();});}
  operator bool()override{Lease l(o_);return l&&o_.slots_[i_].state==Live;}
 };
 class Proxy:public fs::FSImpl {
  Owner&o_;
 public:explicit Proxy(Owner&o):o_(o){}
  fs::FileImplPtr open(const char*p,const char*m,bool c)override{return o_.open(p,m,c);}
  bool exists(const char*p)override{return o_.filesystem<bool>(false,[&]{return o_.backend_.exists(p);});}
  bool rename(const char*a,const char*b)override{return o_.filesystem<bool>(false,[&]{return o_.backend_.rename(a,b);});}
  bool remove(const char*p)override{return o_.filesystem<bool>(false,[&]{return o_.backend_.remove(p);});}
  bool mkdir(const char*p)override{return o_.filesystem<bool>(false,[&]{return o_.backend_.mkdir(p);});}
  bool rmdir(const char*p)override{return o_.filesystem<bool>(false,[&]{return o_.backend_.rmdir(p);});}
 };
 template<class R,class F>R filesystem(R denied,F f){Lease l(*this);if(!l)return denied;R result=f();return ready()?result:denied;}
 template<class R,class F>R query(std::size_t i,R denied,F f){Lease l(*this);if(!l||slots_[i].state!=Live)return denied;R result=f(slots_[i].file);return ready()?result:denied;}
 template<class F>void visit(std::size_t i,F f){Lease l(*this);if(l&&slots_[i].state==Live)f(slots_[i].file);}
 std::size_t reserve(){for(std::size_t i=0;i<Capacity;++i)if((slots_[i].state==Free||slots_[i].state==Closed)&&!slots_[i].block.busy.load(std::memory_order_acquire)){slots_[i].state=Preparing;return i;}return Capacity;}
 bool copyPath(Slot&s,const char*p){if(!p)return false;std::size_t n=0;while(n<PathBytes&&p[n])++n;if(n==PathBytes)return false;std::memcpy(s.path,p,n+1);return true;}
 fs::FileImplPtr finishOpen(std::size_t i,std::shared_ptr<Node>&node,bool child){Slot&s=slots_[i];
  if(!ready()){s.state=Held;return {};}
  if(!s.file){s.file=fs::File();s.state=Free;return {};}
  if(child){const char*p=s.file.path();if(!ready()){s.state=Held;return {};}if(!copyPath(s,p)){trip(Fault::Path);s.state=Held;return {};}}
  s.state=Live;node->armed=true;return node;
 }
 fs::FileImplPtr open(const char*p,const char*m,bool create){Lease l(*this);if(!l||!p||!m)return {};
  // Validate path before reserving a control block or calling the backend.
  std::size_t n=0;while(n<PathBytes&&p[n])++n;if(n==PathBytes)return {};
  const auto i=reserve();if(i==Capacity)return {};auto node=std::allocate_shared<Node>(Allocator<Node>(&slots_[i].block),*this,i);
  copyPath(slots_[i],p);slots_[i].file=backend_.open(p,m,create);return finishOpen(i,node,false);
 }
 fs::FileImplPtr openNext(std::size_t parent,const char*m){Lease l(*this);if(!l||slots_[parent].state!=Live)return {};
  const auto i=reserve();if(i==Capacity)return {};auto node=std::allocate_shared<Node>(Allocator<Node>(&slots_[i].block),*this,i);
  slots_[i].file=slots_[parent].file.openNextFile(m);return finishOpen(i,node,true);
 }
 void release(std::size_t i){Lease l(*this);Slot&s=slots_[i];if(s.state!=Live)return;
  if(!l){s.state=Held;return;}
  // A shadow reference prevents a backend destructor if stop occurs in close.
  s.shadow=s.file;s.file.close();
  if(!ready()){s.state=Held;return;}
  s.shadow=fs::File();s.state=Closed;
 }
 size_t write(std::size_t i,const uint8_t*p,size_t n){Lease l(*this);if(!l||slots_[i].state!=Live)return 0;
  const size_t done=slots_[i].file.write(p,n);if(done!=n)trip(Fault::ShortWrite);return ready()?done:0;
 }
 size_t read(std::size_t i,uint8_t*p,size_t n){Lease l(*this);if(!l||slots_[i].state!=Live)return 0;const size_t got=slots_[i].file.read(p,n);if(got>n)trip(Fault::ReadCount);return ready()?got:0;}
 const char*cachedPath(std::size_t i,bool basename){Lease l(*this);if(!l||slots_[i].state!=Live)return nullptr;const char*p=slots_[i].path;if(basename){const char*s=std::strrchr(p,'/');if(s)p=s+1;}return p;}
public:
 explicit Owner(Backend&b,Mutex&m):backend_(b),mutex_(m){view_=fs::FS(std::allocate_shared<Proxy>(Allocator<Proxy>(&fsBlock_),*this));}
 ~Owner()=delete;Owner(const Owner&)=delete;Owner&operator=(const Owner&)=delete;
 bool enable(){State old=State::Cold;return state_.compare_exchange_strong(old,State::Ready,std::memory_order_release);}
 bool ready()const{return state_.load(std::memory_order_acquire)==State::Ready;}
 void trip(Fault why=Fault::External){if(why==Fault::None)why=Fault::External;Fault old=Fault::None;fault_.compare_exchange_strong(old,why);state_.store(State::Stopped,std::memory_order_release);}
 Fault fault()const{return fault_.load(std::memory_order_acquire);}
 // Not callable from an admitted operation/Lease: mutex is non-recursive.
 bool drained(){mutex_.lock();bool stopped=!ready();mutex_.unlock();return stopped;}
 fs::FS&fs(){return view_;}
 size_t totalBytes(){return filesystem<size_t>(0,[&]{return backend_.totalBytes();});}
 size_t usedBytes(){return filesystem<size_t>(0,[&]{return backend_.usedBytes();});}
 std::size_t retained(){Lease l(*this);std::size_t n=0;for(auto&s:slots_)if(s.state==Held||(!ready()&&s.state==Live))++n;return n;}
};
// No destructor for T is ever invoked. Permanent itself contains only bytes.
template<class T> struct Permanent {
 alignas(T) unsigned char bytes[sizeof(T)];bool initialized=false;
 Permanent()=default;Permanent(const Permanent&)=delete;Permanent&operator=(const Permanent&)=delete;
 template<class...A>T&construct(A&&...a){if(initialized)std::abort();initialized=true;return *new(bytes)T(std::forward<A>(a)...);}
};
}
