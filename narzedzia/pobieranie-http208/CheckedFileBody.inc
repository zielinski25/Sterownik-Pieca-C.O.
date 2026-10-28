#pragma once
#include <cstddef>
#include <algorithm>
namespace history199 {
// Fixed-length, no templates. expected must come from a qualified size snapshot.
// No network methods, no ownership of request, no close/retry on error.
template<class Owner,class File>class CheckedFileBody {
 Owner&owner_;File file_;std::size_t remaining_;bool failed_;
public:
 CheckedFileBody(Owner&owner,const File&file,std::size_t expected):owner_(owner),file_(file),remaining_(expected),failed_(!owner.ready()||!file_){}
 bool valid()const{return !failed_&&owner_.ready();}
 std::size_t read(unsigned char*buffer,std::size_t capacity){
  if(!valid()){failed_=true;return 0;}
  if(!remaining_)return 0;
  if(!buffer||!capacity){failed_=true;return 0;}
  const std::size_t wanted=std::min(capacity,remaining_);
  const std::size_t got=file_.read(buffer,wanted);
  if(!owner_.ready()||got==0||got>wanted){failed_=true;return 0;}
  remaining_-=got;return got;
 }
};
}
