#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include "DownloadAdapter.h"
#ifndef HISTORY203_DEFERRED_POLL
#error "Requires the qualified 203 request hook patch"
#endif

namespace download209 {
// Compares the presented bearer token with the configured one without leaving at the
// first difference, so the comparison time does not depend on how much matched.
inline bool equalConstantTime(const char *a, std::size_t an, const char *b, std::size_t bn) {
 std::size_t diff = an ^ bn;
 const std::size_t n = (an < bn) ? bn : an;
 for (std::size_t i = 0; i < n; ++i) {
  const unsigned char ca = (i < an) ? static_cast<unsigned char>(a[i]) : 0;
  const unsigned char cb = (i < bn) ? static_cast<unsigned char>(b[i]) : 0;
  diff |= static_cast<std::size_t>(ca ^ cb);
 }
 return diff == 0;
}

// Production-shaped download handler: the only place that decides whether a request
// becomes a download. Registered with AsyncURIMatcher::exact(path) and HTTP_ALL, so the
// method decision belongs here and a wrong method is answered, not silently dropped.
//
// Decision order: method -> credentials -> pool -> storage -> response.
// Nothing touches the history file when a decision is a refusal.
template <class Adapter>
class Route {
public:
 struct Stats {
  unsigned downloads = 0, unauthorized = 0, method = 0, busy = 0, unavailable = 0, conflict = 0, internal = 0;
 };
 enum class Outcome { Download, Unauthorized, MethodNotAllowed, Busy, Unavailable, Conflict, Internal };

private:
 Adapter &adapter_;
 std::string token_;
 std::uint32_t timeout_;
 Stats stats_;

 Outcome fail(Outcome o, int code, class AsyncWebServerRequest &r) {
  r.send(code);
  return o;
 }

public:
 Route(Adapter &adapter, std::string token, std::uint32_t timeout) : adapter_(adapter), token_(std::move(token)), timeout_(timeout) {
  if (token_.empty()) token_ = " ";
 }

 const Stats &stats() const { return stats_; }

 // Called from the request/network context with the whole request line and headers
 // already parsed by the pinned library.
 Outcome handle(class AsyncWebServerRequest &r) {
  if (r.method() != AsyncWebRequestMethod::HTTP_GET) {
   ++stats_.method;
   return fail(Outcome::MethodNotAllowed, 405, r);
  }
  if (r.authType() != AUTH_BEARER || !equalConstantTime(r.authChallenge().c_str(), r.authChallenge().length(), token_.c_str(), token_.size())) {
   ++stats_.unauthorized;
   return fail(Outcome::Unauthorized, 401, r);
  }
  const auto begun = adapter_.begin(r, timeout_, true);
  switch (begun.code) {
   case Adapter::Code::Accepted: break;
   case Adapter::Code::Forbidden:
    ++stats_.unauthorized;
    return fail(Outcome::Unauthorized, 401, r);
   case Adapter::Code::Full:
    ++stats_.busy;
    return fail(Outcome::Busy, 503, r);
   case Adapter::Code::Unavailable:
    ++stats_.unavailable;
    return fail(Outcome::Unavailable, 503, r);
   case Adapter::Code::Conflict:
    ++stats_.conflict;
    return fail(Outcome::Conflict, 409, r);
   default:
    ++stats_.internal;
    return fail(Outcome::Internal, 500, r);
  }
  if (!adapter_.finish(begun.handle, r)) {
   ++stats_.internal;
   return fail(Outcome::Internal, 500, r);
  }
  ++stats_.downloads;
  return Outcome::Download;
 }
};
}
