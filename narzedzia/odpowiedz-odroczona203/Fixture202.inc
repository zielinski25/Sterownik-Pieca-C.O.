// Exact test fixture prefix from 202.
#include "UploadPump.h"
#include "Fixture201.inc"
using upload202::Admission;using upload202::Step;
template<size_t F=8,size_t B=32>struct Combined:Fixture {
 Lock queueMutex;using Queue=upload202::Pump<Registry,2,F,B>;
 history197::Permanent<Queue>qs;Queue&q;Combined():q(qs.construct(r,queueMutex)){}
};
