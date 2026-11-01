#pragma once
#include "CheckedFileBody.h"
#ifndef HISTORY199_RESPONSE_GUARD
#error "CheckedFileResponse requires the qualified 199 library patch"
#endif
// Include WebResponseImpl.h before this header. Requires the supplied library
// patch: stock send loop checks _sourceValid only BEFORE filling its buffer.
// Prototype only: no route binding, Content-Disposition, ranges or snapshots.
namespace history199 {
template<class Owner>class CheckedFileResponse final:public AsyncAbstractResponse {
 CheckedFileBody<Owner,fs::File>body_;
public:
 CheckedFileResponse(Owner&owner,const fs::File&file,std::size_t expected)
 :AsyncAbstractResponse(nullptr),body_(owner,file,expected){
  _code=200;_contentType="application/octet-stream";_contentLength=expected;
  _sendContentLength=true;_chunked=false;
 }
 bool _sourceValid()const override{return body_.valid();}
 size_t _fillBuffer(uint8_t*buffer,size_t maxLen)override{return body_.read(buffer,maxLen);}
};
}
