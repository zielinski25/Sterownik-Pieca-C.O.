#pragma once
namespace upload205 {
// Called ONLY by the selected route's whole-request callback, on network context.
// This is not a replacement for header-time authorization/admission/routing.
template<class Adapter>bool end(Adapter&a,typename Adapter::Handle h,AsyncWebServerRequest&r){
 if(!r.strictMultipartComplete205()){r.abort();return false;}
 return a.end(h,r);
}
}
