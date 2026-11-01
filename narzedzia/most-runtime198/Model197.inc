// Model from 197; event preallocation reduced, no behavior change.
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
 Model(){events.reserve(256);files["/a"]="OLD";files["/b"]="KEEP";files["/dir"]="";children={"/a","/b"};}
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
