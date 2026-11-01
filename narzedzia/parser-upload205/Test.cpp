#include "Model203.inc"
#include "UploadAdapter.h"
#include "StrictMultipartEnd.h"
struct Harness:Combined<16,256>,ModelHandler {
 Stats stats;AsyncWebServer server{&stats};Services services{stats};
 using Adapter=upload204::Adapter<Queue,Services>;history197::Permanent<Adapter>as;Adapter&a;Adapter::Handle handle{};
 unsigned uploads=0,finals=0,ends=0,rejections=0;std::string delivered;std::function<void(AsyncWebServerRequest*)>onUpload;
 Harness():a(as.construct(q,services)){}
 void handleUpload(AsyncWebServerRequest*r,const String&name,size_t index,uint8_t*data,size_t length,bool final)override{
  ++uploads;if(final)++finals;delivered.append(reinterpret_cast<const char*>(data),length);
  if(a.chunk(handle,*r,name.c_str(),name.length(),index,data,length,final)!=Admission::Accepted)++rejections;
  if(onUpload)onUpload(r);
 }
 void handleRequest(AsyncWebServerRequest*r)override{++ends;
#ifdef ORIGINAL_PARSER
  assert(a.end(handle,*r));
#else
  assert(upload205::end(a,handle,*r));
#endif
 }
 std::shared_ptr<AsyncWebServerRequest>start(size_t wire,size_t expected,bool strict=true){
  auto r=AsyncWebServerRequest::create(&server,new AsyncClient(stats));r->_handler=this;r->_boundary="AaB03x";r->_contentLength=wire;
#ifndef ORIGINAL_PARSER
  if(strict)assert(r->enableStrictMultipart205(8192));
#else
  (void)strict;
#endif
  auto b=a.begin(*r,expected,true,100);assert(b.code==Adapter::Code::Accepted);handle=b.handle;return r;
 }
};
static Harness*volatile roots[2048]{};static unsigned used=0;
static Harness&fresh(){assert(used<2048);auto*f=new Harness;roots[used++]=f;return *f;}
template<class Q>static void drain(Q&q){for(unsigned i=0;i<1000;++i){auto step=q.runOne();assert(step!=Step::Busy);if(step==Step::Idle)return;}assert(false);}
static void finish(std::shared_ptr<AsyncWebServerRequest>&r){if(!r->disconnected203())r->client()->close();r.reset();}
static std::string body(const std::string&data){return "--AaB03x\r\nContent-Disposition: form-data; name=\"file\"; filename=\"new\"\r\nContent-Type: application/octet-stream\r\n\r\n"+data+"\r\n--AaB03x--\r\n";}
static void feed(Harness&f,std::shared_ptr<AsyncWebServerRequest>r,const std::string&s,size_t block,bool worker=true){
 for(size_t i=0;i<s.size()&&!r->disconnected203();i+=block){std::string part=s.substr(i,block);r->_onData(&part[0],part.size());if(worker)drain(f.q);}
}
int main(){unsigned groups=0;
#ifdef ORIGINAL_PARSER
 // Exact original parser, same adapter: malformed/truncated suffix and length rewrite.
 for(unsigned mode=0;mode<3;++mode){auto&f=fresh();auto wire=body("abc");if(mode==0)wire.replace(wire.size()-4,4,"-XYZ");if(mode==1)wire.resize(wire.size()-4);size_t declared=wire.size()+(mode==2?50:0);auto r=f.start(declared,3);feed(f,r,wire,wire.size());r->_onPoll();assert(f.finals==1&&f.ends==1&&f.stats.status==200);if(mode==2)assert(r->_contentLength<declared);finish(r);drain(f.q);++groups;}
 std::cout<<"PASS original vulnerability reproduced cases="<<groups<<"; not a production fix\n";
#else
 // Every split position plus single-byte reception, including empty and binary files.
 for(const std::string& data:{std::string(),std::string("abc"),std::string("x\0y\r\nz",7)}){
  auto wire=body(data);
  for(size_t split=1;split<wire.size();++split){auto&f=fresh();auto r=f.start(wire.size(),data.size());feed(f,r,wire.substr(0,split),wire.size());if(split<wire.size())assert(f.finals==0&&f.ends==0);feed(f,r,wire.substr(split),wire.size());r->_onPoll();assert(f.finals==1&&f.ends==1&&f.stats.status==200&&f.delivered==data&&f.model.files["/new"]==data&&r->_contentLength==wire.size());finish(r);drain(f.q);}
  auto&f=fresh();auto r=f.start(wire.size(),data.size());feed(f,r,wire,1);r->_onPoll();assert(f.stats.status==200&&f.delivered==data&&f.model.files["/new"]==data);finish(r);drain(f.q);++groups;
 }
 // Each trailer byte is mandatory, no legacy Content-Length rewrite, no 200.
 for(unsigned mode=0;mode<9;++mode){auto&f=fresh();auto wire=body("abc");size_t declared=wire.size();if(mode<4)wire[wire.size()-4+mode]='X';if(mode>=4&&mode<=7){wire.resize(wire.size()-(mode-3));declared=wire.size();}if(mode==8)declared+=50;auto r=f.start(declared,3);feed(f,r,wire,wire.size());r->_onPoll();assert(f.stats.status!=200&&f.finals==0&&f.ends==0&&f.stats.aborts==1&&r->_contentLength==declared);finish(r);drain(f.q);++groups;}
 // Missing transport tail with original length waits, then times out without success.
 {auto&f=fresh();auto wire=body("abc");auto r=f.start(wire.size(),3);wire.resize(wire.size()-2);feed(f,r,wire,1);assert(!f.finals&&!f.ends&&!f.stats.aborts);f.stats.now=100;r->_onPoll();assert(f.stats.aborts==1&&f.stats.status!=200);finish(r);drain(f.q);++groups;}
 // Wrong first boundary / no file / a second part are explicitly outside strict protocol.
 for(unsigned mode=0;mode<3;++mode){auto&f=fresh();auto wire=body("abc");if(mode==0)wire[2]='Z';if(mode==1){auto begin=wire.find("; filename=");auto end=wire.find("\r\n\r\n");wire.erase(begin,end-begin);}if(mode==2)wire.replace(wire.size()-4,4,"\r\nContent-Disposition: form-data; name=\"x\"\r\n\r\ny\r\n--AaB03x--\r\n");auto r=f.start(wire.size(),3);feed(f,r,wire,1);assert(f.stats.aborts==1&&f.finals==0&&f.ends==0);finish(r);drain(f.q);++groups;}
 // Boundary-like prefixes in binary data rewind without losing bytes.
 for(const std::string& data:{std::string("x\rQ\r\nQ\r\n-Q\r\n--AaB0Qy"),std::string(4096,'z')}){auto&f=fresh();auto wire=body(data);auto r=f.start(wire.size(),data.size());feed(f,r,wire,127);r->_onPoll();assert(f.stats.status==200&&f.delivered==data&&f.model.files["/new"]==data);finish(r);drain(f.q);++groups;}
 // Full boundary plus invalid suffix inside data is rejected, never silently truncated.
 {auto&f=fresh();std::string data="a\r\n--AaB03xQb";auto wire=body(data);auto r=f.start(wire.size(),data.size());feed(f,r,wire,1);assert(f.stats.aborts==1&&f.finals==0&&f.ends==0);finish(r);drain(f.q);++groups;}
 // Queue overload stays 503 even with valid framing; network callbacks do no storage I/O.
 {auto&f=fresh();std::string data(4096,'z');auto wire=body(data);auto r=f.start(wire.size(),data.size());feed(f,r,wire,wire.size(),false);assert(f.raw.calls==0&&f.rejections>0);drain(f.q);r->_onPoll();assert(f.stats.status==503&&f.finals==1&&f.ends==1);finish(r);drain(f.q);++groups;}
 // Callback disconnect halts parser before another callback or whole-request completion.
 {auto&f=fresh();auto wire=body(std::string(3000,'z'));auto r=f.start(wire.size(),3000);f.onUpload=[](AsyncWebServerRequest*r){r->client()->close();};feed(f,r,wire,wire.size(),false);assert(f.uploads==1&&f.ends==0&&f.finals==0&&f.raw.calls==0);std::string again="abc";r->_onData(&again[0],again.size());assert(f.uploads==1);f.onUpload={};finish(r);drain(f.q);assert(f.raw.calls==0);++groups;}
 // Destroy request during upload callback; real _onData strong pin survives return.
 {auto&f=fresh();auto wire=body(std::string(1600,'z'));auto r=f.start(wire.size(),1600);auto*raw=r.get();auto weak=AsyncWebServerRequestPtr(r);f.onUpload=[](AsyncWebServerRequest*r){r->client()->close();};r.reset();raw->_onData(&wire[0],wire.size());assert(weak.expired()&&f.uploads==1&&f.ends==0);f.onUpload={};drain(f.q);++groups;}
 // Header-time opt-in validation (header values are injected, not parsed here).
 for(unsigned mode=0;mode<7;++mode){auto&f=fresh();auto r=f.start(123,3,false);if(mode==0)r->_isMultipart=false;if(mode==1)r->_chunkedParseState=CHUNK_LENGTH;if(mode==2)r->_boundary="";if(mode==3)r->_boundary=std::string(71,'x');if(mode==4)r->_contentLength=8193;if(mode==5)r->_parsedLength=1;if(mode==6)r->_parseState=PARSE_REQ_START;assert(!r->enableStrictMultipart205(8192));finish(r);drain(f.q);++groups;}
 // Route end guard refuses unvalidated framing, even if called erroneously early.
 {auto&f=fresh();auto r=f.start(123,3);assert(!upload205::end(f.a,f.handle,*r));assert(f.stats.aborts==1);finish(r);drain(f.q);++groups;}
 // Successful valid legacy mode remains available, strict end guard is intentionally not used.
 {auto&f=fresh();auto wire=body("abc");auto r=f.start(wire.size(),3,false);struct Legacy:ModelHandler{Harness&f;explicit Legacy(Harness&x):f(x){}void handleUpload(AsyncWebServerRequest*r,const String&n,size_t i,uint8_t*d,size_t z,bool last)override{f.handleUpload(r,n,i,d,z,last);}void handleRequest(AsyncWebServerRequest*r)override{++f.ends;assert(f.a.end(f.handle,*r));}} handler(f);r->_handler=&handler;feed(f,r,wire,1);r->_onPoll();assert(f.stats.status==200);finish(r);drain(f.q);++groups;}
 // Every premature declared body end must fail before a final file notification.
 {auto wire=body("abc");for(size_t cut=1;cut<wire.size();++cut){auto&f=fresh();auto r=f.start(cut,3);feed(f,r,wire.substr(0,cut),1);assert(f.stats.aborts==1&&f.finals==0&&f.ends==0);finish(r);drain(f.q);}++groups;}
 // Real parser buffer-allocation failure cancels the admitted adapter token.
 {auto&f=fresh();auto wire=body("abc");auto r=f.start(wire.size(),3);failUploadAlloc205=true;feed(f,r,wire,wire.size());assert(!failUploadAlloc205&&f.stats.aborts==1&&f.uploads==0&&f.ends==0&&f.raw.calls==0);finish(r);drain(f.q);++groups;}
 // A complete second file is rejected before its payload reaches the queue.
 {auto&f=fresh();auto wire=body("abc");auto second=body("def");wire.replace(wire.size()-4,4,"\r\n"+second.substr(std::string("--AaB03x\r\n").size()));auto r=f.start(wire.size(),3);feed(f,r,wire,1);assert(f.stats.aborts==1&&f.finals==0&&f.ends==0&&f.delivered=="abc");finish(r);drain(f.q);++groups;}
 // Boundaries at the allowed extremes; String/header parsing remain modeled.
 for(size_t size:{size_t(1),size_t(70)}){auto&f=fresh();auto wire=body("abc");std::string boundary(size,'Q');size_t i=0;while((i=wire.find("AaB03x",i))!=std::string::npos){wire.replace(i,6,boundary);i+=boundary.size();}auto r=f.start(wire.size(),3,false);r->_boundary=boundary;assert(r->enableStrictMultipart205(8192));feed(f,r,wire,1);r->_onPoll();assert(f.stats.status==200&&f.finals==1);finish(r);drain(f.q);++groups;}
 // Declared epilogue is rejected. Bytes beyond Content-Length are still discarded by upstream _onData.
 {auto&f=fresh();auto wire=body("abc")+"junk";auto r=f.start(wire.size(),3);feed(f,r,wire,wire.size());assert(f.stats.aborts==1&&!f.finals&&!f.ends);finish(r);drain(f.q);++groups;}
 {auto&f=fresh();auto wire=body("abc");auto r=f.start(wire.size(),3);wire+="junk";feed(f,r,wire,wire.size());r->_onPoll();assert(f.stats.status==200);finish(r);drain(f.q);++groups;}
 std::cout<<"PASS parser205 groups="<<groups<<" fixtures="<<used<<"; real body parser + adapter204; modeled headers/String/TCP, not ESP\n";
#endif
}
