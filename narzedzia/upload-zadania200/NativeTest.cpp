#include "UploadPool.h"
#include "Model197.inc"
#include <fstream>
struct Native:history193::Calls {
 std::string root;history193::SystemCalls sys;
 explicit Native(const char*p):root(p){}
 int open(const char*p,int flags)override{assert(std::strncmp(p,"/littlefs/",10)==0);return sys.open((root+"/"+(p+10)).c_str(),flags);}
 int stat(int f,struct stat*s)override{return sys.stat(f,s);}off_t seek(int f,off_t p)override{return sys.seek(f,p);}
 ssize_t read(int f,void*p,size_t n)override{return sys.read(f,p,n);}ssize_t write(int f,const void*p,size_t n)override{return sys.write(f,p,n);}
 int sync(int f)override{return sys.sync(f);}int close(int f)override{return sys.close(f);}
 int rename(const char*,const char*)override{std::abort();}int error()const override{return sys.error();}
};
struct Fixture {Model model;Lock lock;Backend backend;history197::Permanent<Owner>os;Owner&o;Native raw;using Pool=upload200::Pool<Owner>;history197::Permanent<Pool>ps;Pool&p;explicit Fixture(const char*dir):backend(model),o(os.construct(backend,lock)),raw(dir),p(ps.construct(o,raw)){assert(o.enable());}};
static Fixture*volatile root=nullptr;
int main(int argc,char**argv){assert(argc==2);root=new Fixture(argv[1]);auto&f=*root;std::string data;for(int i=0;i<1000;++i)data.push_back(static_cast<char>(i%256));
 errno=EIO;auto t=f.p.begin("native.bin",10,data.size());errno=EIO;assert(f.p.chunk(t,0,reinterpret_cast<const unsigned char*>(data.data()),333,false));errno=EIO;assert(f.p.chunk(t,333,reinterpret_cast<const unsigned char*>(data.data()+333),667,true));errno=EIO;assert(f.p.complete(t).status==200&&f.p.release(t));
 std::ifstream input(std::string(argv[1])+"/native.bin",std::ios::binary);std::string got((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());assert(got==data);
 t=f.p.begin("native.bin",10,0);assert(f.p.complete(t).status==409&&f.p.release(t)&&f.o.ready());
 t=f.p.begin("partial.bin",11,3);const unsigned char b=42;assert(f.p.chunk(t,0,&b,1,false));assert(f.p.release(t)&&!f.p.complete(t).emit);
 struct stat st{};assert(::stat((std::string(argv[1])+"/partial.bin").c_str(),&st)==0&&st.st_size==1);
 std::cout<<"PASS native POSIX upload200: checked binary upload, existing file unchanged, partial file retained; path translation, NOT ESP VFS\n";
}
