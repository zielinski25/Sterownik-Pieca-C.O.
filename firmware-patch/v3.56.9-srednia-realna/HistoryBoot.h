#pragma once
#include "HistoryStartup.h"
#include "Storage.h"
namespace history195 {
enum class Failure { None, Mount, Sequence, Stage, ProbeIO, Transition, Startup };
// Exactly one setup caller, before every history user. No public reset/retry.
// Readiness belongs to Startup194; consumers never see A1's intermediate state.
class Boot {
 history193::Calls&probe_;history_a190::Storage&store_;history194::Startup&startup_;
 bool called_=false;Failure failure_=Failure::None;history_a190::Outcome outcome_{};
 bool stop(Failure f){failure_=f;startup_.runtimeFault();return false;}
 bool absent(const char*p){int fd=probe_.open(p,O_RDONLY);int err=probe_.error();
  if(fd<0){if(err==ENOENT)return true;return stop(Failure::ProbeIO);}
  // Close an owned descriptor exactly once even after an ambiguous open.
  const int closed=probe_.close(fd);const int closeError=probe_.error();
  return stop(err||closed!=0||closeError?Failure::ProbeIO:Failure::Stage);
 }
public:
 Boot(history193::Calls&p,history_a190::Storage&s,history194::Startup&g):probe_(p),store_(s),startup_(g){}
 Failure failure()const{return failure_;}
 history_a190::Outcome transition()const{return outcome_;}
 bool start(bool mounted,history194::Sizes&out){
  if(called_)return false;
  called_=true;
  if(startup_.state()!=history194::State::Cold)return stop(Failure::Sequence);
  if(!mounted)return stop(Failure::Mount);
  if(!absent("/littlefs/hist_short.csv.rebuild")||!absent("/littlefs/hist_long.csv.rebuild"))return false;
  history_a190::Transition transition;outcome_=transition.run(store_);
  if(outcome_.result==history_a190::Result::Blocked)return stop(Failure::Transition);
  if(!startup_.start(true,out))return stop(Failure::Startup);
  return true;
 }
};
}
