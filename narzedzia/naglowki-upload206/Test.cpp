#include "Arduino.h"
#include "literals.h"
#include "Enums206.inc"
using namespace asyncsrv;
#include "Fixture206.inc"
#include "UploadAdapter.h"
#include "StrictMultipartEnd.h"
using upload202::Step;

template<class A>struct Handler206:AsyncWebHandler{
 A&a;typename A::Handle handle{};bool rejecting=false;int code=0;unsigned chunks=0,ends=0,finals=0;
 explicit Handler206(A&x):a(x){}
 bool canHandle(AsyncWebServerRequest*r)const override{return r->url()=="/upload"&&r->method()==AsyncWebRequestMethod::HTTP_POST;}
 bool isRequestHandlerTrivial()override{return rejecting;}
 void handleRequest(AsyncWebServerRequest*r)override{++ends;if(rejecting){r->send(code,T_text_plain,"refused");return;}if(!upload205::end(a,handle,*r))r->abort();}
 void handleUpload(AsyncWebServerRequest*r,const String&name,size_t index,uint8_t*data,size_t len,bool final)override{
  ++chunks;if(final)++finals;if(rejecting)return;a.chunk(handle,*r,name.c_str(),name.length(),index,data,len,final);
 }
};
struct Harness:Combined<16,256>{
 Stats stats;AsyncWebServer server{&stats};Services services{stats};
 using Adapter=upload204::Adapter<Queue,Services>;history197::Permanent<Adapter>as;Adapter&a;
 Handler206<Adapter>handler;size_t expected=3;std::string token="tok";
 unsigned accepted=0,refused=0,framing=0;
 Harness():a(as.construct(q,services)),handler(a){server.handlers={&handler};assert(server.setAttachPolicy206(policy,&*this));}
 static bool policy(AsyncWebServerRequest*r,void*ctx){
  auto&h=*static_cast<Harness*>(ctx);
  if(r->url()!="/upload"||r->method()!=AsyncWebRequestMethod::HTTP_POST)return true;
  if(r->authType()!=AUTH_BEARER||r->authChallenge()!=h.token.c_str()){h.handler.rejecting=true;h.handler.code=403;++h.refused;return true;}
  if(!canonical(r->header("Content-Length"),r->contentLength())||!r->multipart()||!r->contentLength()||r->contentLength()>8192){++h.framing;return false;}
  if(!r->enableStrictMultipart205(8192)){++h.framing;return false;}
  h.handler.rejecting=false;h.handler.code=0;
  auto b=h.a.begin(*r,h.expected,true,100);
  if(b.code!=Adapter::Code::Accepted){h.handler.rejecting=true;h.handler.code=409;++h.refused;return true;}
  h.handler.handle=b.handle;++h.accepted;return true;
 }
 // Canonical decimal only: no sign, no space, no overflow, exact match with parsed length.
 static bool canonical(const String&s,size_t length){
  std::string v=s.c_str();if(v.empty()||v.size()>10||(v.size()>1&&v[0]=='0'))return false;
  unsigned long long n=0;for(char c:v){if(c<'0'||c>'9')return false;n=n*10+(unsigned)(c-'0');}
  return n==length;
 }
};
static Harness*volatile roots[4096]{};static unsigned used=0;
static Harness&fresh(){assert(used<4096);auto*h=new Harness;roots[used++]=h;return *h;}
template<class Q>static void drain(Q&q){for(unsigned i=0;i<1000;++i){auto step=q.runOne();assert(step!=Step::Busy);if(step==Step::Idle)return;}assert(false);}
static void finish(std::shared_ptr<AsyncWebServerRequest>r){if(r&&!r->disconnected203())r->client()->close();r.reset();}
static std::string multipart(const std::string&data,const std::string&boundary="AaB03x",const std::string&filename="new"){
 return "--"+boundary+"\r\nContent-Disposition: form-data; name=\"file\"; filename=\""+filename+"\"\r\nContent-Type: application/octet-stream\r\n\r\n"+data+"\r\n--"+boundary+"--\r\n";
}
static std::string wire(const std::string&body,const std::string&boundary="AaB03x",const std::string&auth="Authorization: Bearer tok\r\n",const std::string&method="POST",const std::string&path="/upload"){
 return method+" "+path+" HTTP/1.1\r\nHost: oven\r\nContent-Type: multipart/form-data; boundary="+boundary+"\r\nContent-Length: "+std::to_string(body.size())+"\r\n"+auth+"\r\n"+body;
}
static void feed(std::shared_ptr<AsyncWebServerRequest>r,const std::string&s,size_t block=0){
 if(!block)block=s.size();
 for(size_t i=0;i<s.size()&&!r->disconnected203();i+=block){std::string part=s.substr(i,block);r->_onData(&part[0],part.size());}
}
static void settle(Harness&h,std::shared_ptr<AsyncWebServerRequest>&r){
 drain(h.q);r->_onPoll();assert(h.stats.status!=200||h.handler.chunks>0);
}
int main(){unsigned groups=0;
 // 1. Real header parsing split at every byte; reservation at attach precedes body.
 {const std::string body=multipart("abc"),w=wire(body);for(size_t cut=1;cut<w.size();++cut){auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));feed(r,w.substr(0,cut));assert(cut>w.find("\r\n\r\n")+4||(h.model.files.count("/new")==0&&h.handler.chunks==0));feed(r,w.substr(cut));settle(h,r);assert(h.stats.status==200&&h.accepted==1&&h.handler.chunks>0);assert(h.model.files["/new"]=="abc");finish(r);}auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));feed(r,w,1);settle(h,r);assert(h.stats.status==200&&h.model.files["/new"]=="abc");finish(r);++groups;}
 // 2. Other route: handler not selected, no policy admission, no storage, 501.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string w=wire(multipart("abc"),"AaB03x","Authorization: Bearer tok\r\n","POST","/other");feed(r,w);assert(h.accepted==0&&h.raw.calls==0&&h.handler.chunks==0&&h.handler.ends==0);assert(h.stats.status==501);finish(r);++groups;}
 // 3. GET /upload is not selected; body discarded, no upload.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string w=wire(multipart("abc"),"AaB03x","Authorization: Bearer tok\r\n","GET");feed(r,w);assert(h.accepted==0&&h.raw.calls==0&&h.handler.chunks==0&&h.stats.status==501);finish(r);++groups;}
 // 4. Unknown method fails at the request line before any handler work.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string w=wire(multipart("abc"),"AaB03x","Authorization: Bearer tok\r\n","BREW");feed(r,w);assert(h.stats.aborts==1&&h.accepted==0&&h.handler.chunks==0&&h.raw.calls==0);finish(r);++groups;}
 // 5. Bad credentials: claimed and refused without parsing bytes or touching storage.
 {for(const char*auth:{"", "Authorization: Bearer nope\r\n", "Authorization: Basic tok\r\n", "Authorization: tok\r\n"}){auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));feed(r,wire(multipart("abc"),"AaB03x",auth));settle(h,r);assert(h.stats.status==403&&h.refused==1&&h.accepted==0);assert(h.handler.chunks==0&&h.raw.calls==0&&h.model.files.count("/new")==0);finish(r);}++groups;}
 // 6. Header names are case-insensitive and OWS tabs/spaces are accepted.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");
  std::string w="POST /upload HTTP/1.1\r\nhOsT:\toven\r\ncOnTeNt-TyPe:  multipart/form-data; boundary=AaB03x\r\ncontent-length:\t"+std::to_string(body.size())+"\r\naUtHoRiZaTiOn:   Bearer tok\r\n\r\n"+body;feed(r,w,3);settle(h,r);assert(h.stats.status==200&&h.accepted==1);finish(r);++groups;}
 // 7. Query/path percent decoding and plus signs use the real urlDecode/_addGetParams.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");std::string w=wire(body,"AaB03x","Authorization: Bearer tok\r\n","POST","/up%6coad?x=a%20b&y=1+2&empty=");feed(r,w);settle(h,r);assert(h.stats.status==200&&r->url()=="/upload"&&r->headers()==4);
  unsigned query=0;for(auto&p:r->_params)if(!p.post)++query;assert(query==3);
  std::string got;for(auto&p:r->_params)if(!p.post)got+=p.n.c_str()+std::string("=")+p.v.c_str()+";";
  assert(got=="x=a b;y=1 2;empty=;");finish(r);++groups;}
 // 8. Duplicate equal Content-Length is accepted only with matching body; conflicting is dropped.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");
  std::string w="POST /upload HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\nContent-Length: "+std::to_string(body.size())+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nAuthorization: Bearer tok\r\n\r\n"+body;feed(r,w);settle(h,r);assert(h.stats.status==200);finish(r);}
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");
  std::string w="POST /upload HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\nContent-Length: "+std::to_string(body.size()+1)+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nAuthorization: Bearer tok\r\n\r\n"+body;feed(r,w);assert(h.stats.aborts==1&&h.framing==1&&h.accepted==0&&h.raw.calls==0);finish(r);}++groups;
 // 9. Non-canonical, negative, missing, oversized and X-Expected-only lengths fail closed.
 {for(unsigned mode=0;mode<5;++mode){auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");
   std::string cl=mode==0?std::string("abc"):mode==1?std::string("-1"):mode==2?std::string(""):mode==3?"99999999999":"  12 ";std::string extra=mode==4?"X-Expected-Entity-Length: "+std::to_string(body.size())+"\r\n":"";
   std::string w="POST /upload HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\n"+(mode==2?extra:"Content-Length: "+cl+"\r\n")+extra+"Authorization: Bearer tok\r\n\r\n"+body;feed(r,w);assert(h.stats.aborts==1&&h.framing==1&&h.accepted==0&&h.raw.calls==0);finish(r);}++groups;}
 // 10. Boundary forms: quoted, escaped quote, semicolon inside quotes and trailing OWS.
 {struct Case{std::string header,boundary;};const Case cases[]={{"multipart/form-data; boundary=\"AaB03x\"","AaB03x"},{"multipart/form-data; boundary=\"Aa\\\"B\"","Aa\"B"},{"multipart/form-data; boundary=\"x; boundary=y\"; boundary=AaB03x","x; boundary=y"},{"multipart/form-data; boundary=AaB03x   ","AaB03x"}};
  for(auto&c:cases){auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc",c.boundary);std::string w="POST /upload HTTP/1.1\r\nContent-Type: "+c.header+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nAuthorization: Bearer tok\r\n\r\n"+body;feed(r,w,1);settle(h,r);assert(h.stats.status==200&&h.accepted==1);finish(r);}++groups;}
 // 11. Empty, over-long, missing and mismatched boundaries never reach storage.
 {for(unsigned mode=0;mode<4;++mode){auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));std::string b=mode==0?std::string(""):mode==1?std::string(71,'x'):std::string("AaB03x");std::string ct=mode==2?"text/plain":"multipart/form-data; boundary="+b;
   std::string body=mode==3?multipart("abc","ZZZ"):multipart("abc",b);
   std::string w="POST /upload HTTP/1.1\r\nContent-Type: "+ct+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nAuthorization: Bearer tok\r\n\r\n"+body;
   feed(r,w);assert(h.stats.aborts==1&&h.accepted==(mode==3?1u:0u)&&h.raw.calls==0);assert(h.stats.status==0&&h.model.files.count("/new")==0);finish(r);}++groups;}
 // 12. NUL and malformed header lines are rejected or ignored before admission.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");std::string w="POST /upload HTTP/1.1\r\nBad\0Header\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\nContent-Length: "+std::to_string(body.size())+"\r\nAuthorization: Bearer tok\r\n\r\n"+body;feed(r,w);assert(h.stats.aborts==1&&h.accepted==0);finish(r);}
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");std::string w="POST /upload HTTP/1.1\r\nNoColonHeader\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\nContent-Length: "+std::to_string(body.size())+"\r\nAuthorization: Bearer tok\r\n\r\n"+body;feed(r,w);settle(h,r);assert(h.stats.status==200&&r->headers()==3);finish(r);}++groups;
 // 13. Expect: 100-continue is written before body, then the upload still succeeds.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");std::string w="POST /upload HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\nContent-Length: "+std::to_string(body.size())+"\r\nAuthorization: Bearer tok\r\nExpect: 100-continue\r\n\r\n"+body;feed(r,w,5);settle(h,r);assert(r->client()==nullptr||r->client()->written=="HTTP/1.1 100 Continue\r\n\r\n");assert(h.stats.status==200&&h.accepted==1);finish(r);++groups;}
 // 14. Chunked transfer encoding is refused by this strict route.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));std::string w="POST /upload HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\nTransfer-Encoding: chunked\r\nAuthorization: Bearer tok\r\n\r\n";feed(r,w);assert(h.stats.aborts==1&&h.accepted==0&&h.raw.calls==0);finish(r);++groups;}
 // 15. Disconnect mid-body frees the pool; the next real request is accepted.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart(std::string(3000,'z')),w=wire(body);feed(r,w.substr(0,w.size()-2000));assert(h.accepted==1&&h.raw.calls==0);r->client()->close();assert(h.refused==0);r.reset();drain(h.q);auto r2=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));feed(r2,wire(multipart("xyz","AaB03x","second"),"AaB03x","Authorization: Bearer tok\r\n"));settle(h,r2);assert(h.accepted==2&&h.stats.status==200&&h.model.files["/second"]=="xyz");finish(r2);++groups;}
 // 16. Fifty sequential real-wire uploads reuse the same permanent pool.
 {auto&h=fresh();for(unsigned i=0;i<50;++i){auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));std::string data="n"+std::to_string(i),name="f"+std::to_string(i);h.expected=data.size();feed(r,wire(multipart(data,"AaB03x",name)));settle(h,r);assert(h.stats.status==200&&h.model.files["/"+name]==data);finish(r);drain(h.q);}assert(h.accepted==50&&h.stats.aborts==0);++groups;}
 // 17. Refused auth does not consume a slot: a valid upload immediately follows.
 {auto&h=fresh();auto bad=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));feed(bad,wire(multipart("abc"),"AaB03x",""));settle(h,bad);assert(h.stats.status==403);finish(bad);auto good=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));feed(good,wire(multipart("abc")));settle(h,good);assert(h.stats.status==200&&h.accepted==1);finish(good);++groups;}
 // 18. Server policy cannot be silently replaced once installed.
 {auto&h=fresh();assert(!h.server.setAttachPolicy206(Harness::policy,&h));h.server.clearAttachPolicy206();assert(h.server.setAttachPolicy206(Harness::policy,&h));++groups;}
 // 19. Boundary text inside data with a bad suffix is rejected; no file-final callback.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string data="a\r\n--AaB03xQb";const std::string body=multipart(data);const size_t declared=body.size();
  feed(r,wire(body));assert(h.stats.aborts==1&&h.handler.finals==0&&h.accepted==1&&h.model.files.count("/new")==0&&r->_contentLength==declared);finish(r);drain(h.q);++groups;}
 // 20. A declared body four bytes longer than the framing is rejected without rewriting it.
 {auto&h=fresh();auto r=AsyncWebServerRequest::create(&h.server,new AsyncClient(h.stats));const std::string body=multipart("abc");std::string w="POST /upload HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=AaB03x\r\nContent-Length: "+std::to_string(body.size()+4)+"\r\nAuthorization: Bearer tok\r\n\r\n"+body;
  feed(r,w);assert(h.stats.aborts==1&&h.handler.finals==0&&h.accepted==1&&r->_contentLength==body.size()+4);finish(r);drain(h.q);++groups;}
 std::cout<<"PASS headers206 groups="<<groups<<" instances="<<used<<"; real pinned header/body parsing + adapter204/205; modeled String/TCP/route match, not ESP\n";
}
