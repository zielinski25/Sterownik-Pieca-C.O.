#pragma once
// One boot owner only, before EVERY history producer/reader starts.
// This is not a runtime HTTP clear operation, nor an authentication mechanism.
namespace history_a190 {
enum class Token { Absent, Valid, Invalid, Fault };
enum class History { Short, Long };
enum class Result { Blocked, Existing, Fresh };
enum class Reason { None, CompletionRead, RequestRead, ShortRemove, LongRemove,
                    CompletionWrite, CompletionVerify, RequestConsume, RequestVerify, Seal };
struct Outcome { Result result; Reason reason;
 Outcome(Result r=Result::Blocked,Reason why=Reason::None):result(r),reason(why){}
};
struct Storage {
 virtual ~Storage()=default;
 virtual Token completion()=0;
 virtual Token request()=0;
 // Success means confirmed absent, including an already absent file.
 virtual bool removeHistory(History)=0;
 // Exclusive creation only; never truncate/replace an existing marker.
 virtual bool createCompletion()=0;
 // Called only after a checked valid completion marker and valid request.
 virtual bool consumeRequest()=0;
 // Re-establish durability even when a previous failed boot left a readable
 // marker and an apparently absent request in the filesystem cache.
 virtual bool sealCompletion()=0;
};
class Transition {
 bool called_=false;
 Outcome out_{};
 Outcome blocked(Reason r){out_={Result::Blocked,r};return out_;}
 Outcome release(Storage&s){
  const auto r=s.request();
  if(r==Token::Invalid||r==Token::Fault)return blocked(Reason::RequestRead);
  if(r==Token::Valid){
   if(!s.consumeRequest())return blocked(Reason::RequestConsume);
   if(s.request()!=Token::Absent)return blocked(Reason::RequestVerify);
  }
  if(!s.sealCompletion())return blocked(Reason::Seal);
  out_={Result::Fresh,Reason::None};return out_;
 }
public:
 // A failure is latched for this instance/boot. No in-place retry.
 Outcome run(Storage&s){
  if(called_)return out_;
  called_=true;
  const auto c=s.completion();
  if(c==Token::Invalid||c==Token::Fault)return blocked(Reason::CompletionRead);
  if(c==Token::Valid)return release(s); // NEVER delete history on this branch.
  const auto r=s.request();
  if(r==Token::Invalid||r==Token::Fault)return blocked(Reason::RequestRead);
  if(r==Token::Absent){out_={Result::Existing,Reason::None};return out_;}
  if(!s.removeHistory(History::Short))return blocked(Reason::ShortRemove);
  if(!s.removeHistory(History::Long))return blocked(Reason::LongRemove);
  if(!s.createCompletion())return blocked(Reason::CompletionWrite);
  if(s.completion()!=Token::Valid)return blocked(Reason::CompletionVerify);
  return release(s);
 }
};
}
