// Exact model Calls from 198; not ESP VFS.
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
