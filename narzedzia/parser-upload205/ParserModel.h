#pragma once
#include <list>
#include <functional>
#include "Enums.inc"
#define async_ws_log_d(...) ((void)0)
#define async_ws_log_e(...) ((void)0)
#define T_app_xform_urlencoded "application/x-www-form-urlencoded"
#define T_Content_Type "Content-Type"
#define T_Content_Disposition "Content-Disposition"
#define T_name "name"
#define T_filename "filename"
// Pinned library default. Test fixture uses this actual upload buffer size.
#define RESPONSE_STREAM_BUFFER_SIZE 1460
namespace asyncsrv {static const String emptyString;}
static inline bool isParamChar(char c){return c&&c!='{'&&c!='['&&c!='&'&&c!='=';}
struct AsyncWebParameter {
 String n,v;bool post,file;size_t size;
 AsyncWebParameter(String a,String b,bool p=false,bool f=false,size_t z=0):n(a),v(b),post(p),file(f),size(z){}
 bool isPost()const{return post;}bool isFile()const{return file;}const String&name()const{return n;}
};
class AsyncWebServerRequest;
struct ModelHandler {
 virtual ~ModelHandler(){}virtual void handleUpload(AsyncWebServerRequest*,const String&,size_t,uint8_t*,size_t,bool)=0;
 virtual void handleRequest(AsyncWebServerRequest*)=0;
 bool isRequestHandlerTrivial(){return false;}
 void handleBody(AsyncWebServerRequest*,uint8_t*,size_t,size_t,size_t){assert(false);}
};

// Fault injection only around the parser allocation, not global C++ allocations.
static bool failUploadAlloc205=false;
static void* uploadAlloc205(size_t n){if(failUploadAlloc205){failUploadAlloc205=false;return nullptr;}return std::malloc(n);}
