#pragma once
#include <cstddef>
namespace history197 {
enum class ReadStatus { Data, End, Stopped, Invalid };
struct ReadResult {ReadStatus status;std::size_t bytes;ReadResult(ReadStatus s,std::size_t n=0):status(s),bytes(n){}};
// Integration aid, NOT an HTTP response implementation. A caller receiving
// Stopped/Invalid must abort, not report a successful shorter download.
// A later fault cannot retract bytes already sent over the network.
template<class Owner,class File,std::size_t N>
ReadResult checkedRead(Owner&owner,File&file,unsigned char(&buffer)[N],std::size_t wanted){
 if(!wanted||wanted>N)return ReadResult(ReadStatus::Invalid);
 if(!owner.ready())return ReadResult(ReadStatus::Stopped);
 const std::size_t got=file.read(buffer,wanted);
 if(!owner.ready())return ReadResult(ReadStatus::Stopped);
 if(got>wanted)return ReadResult(ReadStatus::Invalid);
 return ReadResult(got?ReadStatus::Data:ReadStatus::End,got);
}
}
