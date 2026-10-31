#include "Model197.inc"
#include <array>
#include <algorithm>
#include <memory>
#ifndef ASYNCWEBSERVER_USE_CHUNK_INFLIGHT
#define ASYNCWEBSERVER_USE_CHUNK_INFLIGHT 0
#endif
#define CONFIG_LWIP_TCP_MSS 32
#define CONFIG_LWIP_TCP_WND_DEFAULT 64
#define ASYNC_RESPONCE_BUFF_SIZE (CONFIG_LWIP_TCP_MSS * 2)
#define RESPONSE_TRY_AGAIN 0xFFFFFFFF
using AwsTemplateProcessor=std::function<String(const String&)>;
enum WebResponseState {RESPONSE_SETUP,RESPONSE_HEADERS,RESPONSE_CONTENT,RESPONSE_WAIT_ACK,RESPONSE_END,RESPONSE_FAILED};
struct Client {
 size_t window=4096,limit=4096,addCalls=0,sends=0,closes=0;bool closed=false,can=true;std::string wire;std::function<void()>onAdd;
 size_t space(){return window;}
 size_t add(const char*p,size_t n){assert(n<=ASYNC_RESPONCE_BUFF_SIZE);++addCalls;if(onAdd)onAdd();size_t got=std::min(n,std::min(window,limit));wire.append(p,got);window-=got;return got;}
 bool send(){++sends;return !closed;}
 void close(){++closes;closed=true;}
 bool canSend(){return can&&!closed;}
};
class AsyncWebServerRequest;
// Model base/storage/network only; real class declaration and methods below.
class AsyncWebServerResponse {
protected:
 int _code=200;String _contentType;size_t _contentLength=0;bool _sendContentLength=true,_chunked=false;
 size_t _sentLength=0,_ackedLength=0,_writtenLength=0;WebResponseState _state=RESPONSE_CONTENT;
public:
 virtual ~AsyncWebServerResponse(){}virtual bool _sourceValid()const{return false;}
 virtual bool _finished()const;virtual void _respond(AsyncWebServerRequest*)=0;virtual size_t _ack(AsyncWebServerRequest*,size_t,uint32_t)=0;
 WebResponseState state()const{return _state;}size_t announced()const{return _contentLength;}
};
struct AsyncWebServerRequest {
 std::shared_ptr<AsyncWebServerRequest>_this;AsyncWebServerResponse*_response;Client*_client;
 AsyncWebServerRequest(AsyncWebServerResponse&r,Client&c):_response(&r),_client(&c){}
 Client*client(){return _client;}void _onAck(size_t,uint32_t);void _onPoll();void _onTimeout(uint32_t);
};
#include "Abstract.inc"
// Only the no-template dispatch branch is modeled; template processor is NOT run.
size_t AsyncAbstractResponse::_fillBufferAndProcessTemplates(uint8_t*b,size_t n){assert(!_callback);return _fillBuffer(b,n);}
// Headers are a four-byte marker, not an HTTP header serialization test.
void AsyncAbstractResponse::_respond(AsyncWebServerRequest*r){_assembled_headers="HEAD";_state=RESPONSE_HEADERS;write_send_buffs(r,0,0);}
#include "Transport.inc"
#include "CheckedFileResponse.h"
struct Fixture {Model model;Lock lock;Backend backend;history197::Permanent<Owner>os;Owner&o;Fixture():backend(model),o(os.construct(backend,lock)){assert(o.enable());}};
static Fixture*roots[1024]{};static unsigned used=0;
static Fixture&fresh(const std::string&data="abc"){assert(used<1024);auto&f=*(roots[used++]=new Fixture);f.model.files["/a"]=data;return f;}
using Response=history199::CheckedFileResponse<Owner>;
struct Dummy:AsyncAbstractResponse {
 std::string data;size_t pos=0,calls=0;int mode=0;bool live=true;
 explicit Dummy(bool chunk=false):data(90,'x'){_chunked=chunk;_sendContentLength=!chunk;_contentLength=chunk?0:data.size();}
 bool _sourceValid()const override{return live;}
 size_t _fillBuffer(uint8_t*b,size_t n)override{++calls;if(mode==1)return n+1;if(mode==2)return size_t(-1);if(mode==3){mode=0;return RESPONSE_TRY_AGAIN;}auto count=std::min(n,data.size()-pos);std::memcpy(b,data.data()+pos,count);pos+=count;return count;}
};
static void finish(AsyncWebServerRequest&r,Client&c){size_t ack=0;for(unsigned i=0;i<1000&&!c.closed;++i){c.window=4096;auto before=c.wire.size();r._onAck(ack,0);ack=c.wire.size()-before;}assert(c.closed&&r._response->_finished());}
int main(){unsigned groups=0;
 // Different sizes and positive partial socket acceptance, including last buffer.
 for(size_t len:{size_t(0),size_t(1),size_t(3),size_t(64),size_t(65),size_t(200)})for(size_t limit:{size_t(1),size_t(7),size_t(4096)}){
  std::string data(len,'x');auto&f=fresh(data);auto file=f.o.fs().open("/a");Client c;c.limit=limit;Response response(f.o,file,len);AsyncWebServerRequest request(response,c);finish(request,c);assert(c.wire==data&&response.state()==RESPONSE_END&&f.o.ready()&&response.announced()==len);++groups;
 }
 // The final pending buffer MUST not look finished to the REAL request ack method.
 {auto&f=fresh();auto file=f.o.fs().open("/a");Client c;c.limit=0;Response response(f.o,file,3);AsyncWebServerRequest request(response,c);request._onAck(0,0);assert(!response._finished()&&!c.closed&&c.wire.empty());c.limit=1;finish(request,c);assert(c.wire=="abc"&&response.state()==RESPONSE_END);++groups;}
 // Stop before first call, during first read, and during final read: no clean EOF/retry.
 for(int where=0;where<3;++where){auto&f=fresh(std::string(90,'x'));auto file=f.o.fs().open("/a");Client c;Response response(f.o,file,90);AsyncWebServerRequest request(response,c);unsigned reads=0;
  if(where==0)f.o.trip();else f.model.hook=[&](const char*op){if(!std::strcmp(op,"read")&&++reads==unsigned(where))f.o.trip();};
  request._onAck(0,0);assert(response.state()==RESPONSE_FAILED&&c.closed&&f.model.closes==0);assert(c.wire.size()==(where==2?64:0));f.model.hook={};++groups;
 }
 // Fault from another task changes only the owner; close belongs to next network call.
 {auto&f=fresh(std::string(90,'x'));auto file=f.o.fs().open("/a");Client c;c.limit=2;Response response(f.o,file,90);AsyncWebServerRequest request(response,c);request._onAck(0,0);assert(c.wire.size()==2&&!c.closed);auto count=f.model.events.size();f.o.trip();assert(!c.closed);request._onAck(2,0);assert(c.closed&&response.state()==RESPONSE_FAILED&&c.wire.size()==2&&f.model.events.size()==count);++groups;}
 // No poll wakeup when canSend is false: explicit limitation, not immediate cancellation.
 {auto&f=fresh();auto file=f.o.fs().open("/a");Client c;c.can=false;Response response(f.o,file,3);AsyncWebServerRequest request(response,c);f.o.trip();request._onPoll();assert(!c.closed);c.can=true;request._onPoll();assert(c.closed&&response.state()==RESPONSE_FAILED);++groups;}
 // Stop inside socket add must prevent another fill or explicit send in this pump.
 {auto&f=fresh();auto file=f.o.fs().open("/a");Client c;c.limit=0;c.onAdd=[&]{f.o.trip();};Response response(f.o,file,3);AsyncWebServerRequest request(response,c);request._onAck(0,0);assert(c.closed&&response.state()==RESPONSE_FAILED&&c.sends==0&&c.wire.empty());c.onAdd={};++groups;}
 // Premature EOF, invalid count, or alias closed: local failure, never successful short body.
 for(int mode=0;mode<3;++mode){auto&f=fresh();auto file=f.o.fs().open("/a");Client c;Response response(f.o,file,mode==0?5:3);AsyncWebServerRequest request(response,c);if(mode==1)f.model.invalidReadCount=true;if(mode==2)file.close();request._onAck(0,0);assert(c.closed&&response.state()==RESPONSE_FAILED);if(mode==0)assert(c.wire=="abc"&&f.o.ready());else assert(c.wire.empty());++groups;}
 // Count guard also covers generic callbacks, both framing modes.
 for(bool chunk:{false,true})for(int mode:{1,2}){Dummy response(chunk);response.mode=mode;Client c;AsyncWebServerRequest request(response,c);request._onAck(0,0);if(mode==2&&sizeof(size_t)==4)assert(!c.closed);else assert(response.state()==RESPONSE_FAILED&&c.closed&&c.wire.empty());++groups;}
 // Healthy chunk framing and one transient retry remain functional (no templates).
 for(bool chunk:{false,true})for(size_t limit:{size_t(1),size_t(7),size_t(4096)}){Dummy response(chunk);response.mode=3;Client c;c.limit=limit;AsyncWebServerRequest request(response,c);request._onPoll();assert(!c.closed);finish(request,c);assert(response.state()==RESPONSE_END);if(chunk)assert(c.wire=="0038\r\n"+std::string(56,'x')+"\r\n0022\r\n"+std::string(34,'x')+"\r\n0000\r\n\r\n");else assert(c.wire==std::string(90,'x'));++groups;}
 // Header fragmentation modeled; actual header serialization not under test.
 {auto&f=fresh();auto file=f.o.fs().open("/a");Client c;c.limit=1;Response response(f.o,file,3);AsyncWebServerRequest request(response,c);response._respond(&request);finish(request,c);assert(c.wire=="HEADabc");++groups;}
 // Already queued complete content cannot be recalled by a later stop.
 {auto&f=fresh();auto file=f.o.fs().open("/a");Client c;Response response(f.o,file,3);AsyncWebServerRequest request(response,c);finish(request,c);f.o.trip();assert(c.wire=="abc"&&response.state()==RESPONSE_END);++groups;}
 // Explicit 32-bit sentinel values, without pretending to execute an ESP ABI.
 for(size_t count:{size_t(0xffffffffu),size_t(-1)}){struct Ready{bool ready()const{return true;}}owner;struct File{size_t n;explicit operator bool()const{return true;}size_t read(unsigned char*,size_t){return n;}}file{count};history199::CheckedFileBody<Ready,File>body(owner,file,3);unsigned char b[3]{};assert(body.read(b,3)==0&&!body.valid());++groups;}
 // Fixed snapshot length does not expose a later suffix or read again at EOF.
 {auto&f=fresh("abcdef");auto file=f.o.fs().open("/a");history199::CheckedFileBody<Owner,fs::File>body(f.o,file,2);uint8_t b[8]{};assert(body.read(b,8)==2&&std::string(reinterpret_cast<char*>(b),2)=="ab");auto calls=f.model.events.size();assert(body.read(b,8)==0&&body.valid()&&f.model.events.size()==calls);++groups;}
 // Invalid caller buffer must not reach File.read.
 {auto&f=fresh();auto file=f.o.fs().open("/a");history199::CheckedFileBody<Owner,fs::File>body(f.o,file,3);auto calls=f.model.events.size();assert(body.read(nullptr,3)==0&&!body.valid()&&f.model.events.size()==calls);++groups;}
 std::cout<<"PASS http199 groups="<<groups<<" flow="<<ASYNCWEBSERVER_USE_CHUNK_INFLIGHT<<"; real patched send loop + request ack/poll; modeled TCP, no templates or ESP\n";
}
