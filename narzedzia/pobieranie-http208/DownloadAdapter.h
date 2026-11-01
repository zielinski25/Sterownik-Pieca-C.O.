#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include "SnapshotResponse.h"
#include "Snapshots.h"
#ifndef HISTORY203_DEFERRED_POLL
#error "Requires the qualified 203 request hook patch"
#endif

namespace download208 {
// Network context only, permanent lifetime, one instance per server.
//
// Pins a snapshot prefix (207) at admission, then sends exactly that prefix with
// a fixed Content-Length through the real 199 send loop. Storage faults invalidate
// the source instead of ending the download early, so a short file is never a 200.
//
// Slot reuse requires disconnect AND weak expiry, so an old response cannot read
// a newer pin. Pin is released on finish, disconnect, deadline, read fault and
// failed response allocation. Nothing here writes, renames or deletes files.
template <class Snapshots, class Services, std::size_t Capacity = 2>
class Adapter {
 static_assert(Capacity > 0 && Capacity < 256, "bounded pool");
public:
 struct Handle { std::size_t slot; std::uint64_t generation; };
 enum class Code { Accepted, Forbidden, Invalid, Full, Unavailable, Conflict };
 struct Begin { Code code; Handle handle; };

private:
 struct Slot;
 class SlotReader final : public Reader {
  Adapter *a_ = nullptr;
  std::size_t i_ = 0;
 public:
  void bind(Adapter *a, std::size_t i) { a_ = a; i_ = i; }
  bool valid() override { return a_->validFor(i_); }
  std::size_t read(unsigned char *b, std::size_t n) override { return a_->readFor(i_, b, n); }
 };
 struct Slot {
  Adapter *owner = nullptr;
  SlotReader reader;
  typename Snapshots::Handle snap{};
  AsyncWebServerRequestPtr request;
  AsyncWebServerResponse *response = nullptr;  // adapter-owned until finish()
  std::uint64_t generation = 0;
  std::uint32_t started = 0, timeout = 0;
  bool occupied = false, disconnected = false, done = false, sent = false;
  mutable bool failed = false;
 };

 Snapshots &snapshots_;
 Services &services_;
 Slot slots_[Capacity];

 static Handle invalid() { return {Capacity, 0}; }
 Slot *find(Handle h, AsyncWebServerRequest &r) {
  if (h.slot >= Capacity || !h.generation) return nullptr;
  Slot &s = slots_[h.slot];
  if (!s.occupied || s.generation != h.generation || s.disconnected) return nullptr;
  auto live = s.request.lock();
  return (live && live.get() == &r) ? &s : nullptr;
 }
 static void closePin(Slot &s) {
  if (s.snap.slot < Capacity && s.snap.generation) { s.owner->snapshots_.close(s.snap); s.snap = {}; }
 }
 // Idempotent. Never deletes a response already handed to the request.
 void release(Slot &s) {
  if (!s.occupied) return;
  closePin(s);
  if (!s.sent && s.response) { delete s.response; s.response = nullptr; }
  s.response = nullptr;
  s.done = true;
  s.occupied = false;
  if (s.request.lock()) s.request.lock()->stopDeferredPoll203();
 }
 static void gone(void *p) {
  Slot &s = *static_cast<Slot *>(p);
  s.disconnected = true;
  s.owner->release(s);
 }
 static void poll(AsyncWebServerRequest *r, void *p) {
  Slot &s = *static_cast<Slot *>(p);
  s.owner->tick(s, *r);
 }
 void tick(Slot &s, AsyncWebServerRequest &r) {
  if (!s.occupied || s.done) return;
  if (r.disconnected203() || !r.client()) { s.disconnected = true; release(s); return; }
  if (s.sent && r.getResponse() && r.getResponse()->_finished()) { release(s); return; }
  if (static_cast<std::uint32_t>(services_.now() - s.started) >= s.timeout) {
   s.disconnected = true;
   release(s);
   r.abort();
  }
 }
 bool validFor(std::size_t i) {
  const Slot &s = slots_[i];
  return s.occupied && !s.failed && !s.disconnected && s.snap.generation && !s.done;
 }
 std::size_t readFor(std::size_t i, unsigned char *buffer, std::size_t n) {
  Slot &s = slots_[i];
  if (!s.occupied || s.failed || s.disconnected || s.done) return 0;
  const auto r = snapshots_.read(s.snap, buffer, n, services_.now());
  if (r.status == download207::ReadStatus::Data) return r.bytes;
  if (r.status == download207::ReadStatus::End) return 0;
  s.failed = true;  // Stopped/Stale/Invalid must never look like a clean end
  return 0;
 }

public:
 Adapter(Snapshots &s, Services &v) : snapshots_(s), services_(v) {
  for (std::size_t i = 0; i < Capacity; ++i) {
   slots_[i].owner = this;
   slots_[i].reader.bind(this, i);
  }
 }
 ~Adapter() = delete;
 Adapter(const Adapter &) = delete;
 Adapter &operator=(const Adapter &) = delete;

 // Admission only. Call finish() to hand the response to the request.
 Begin begin(AsyncWebServerRequest &r, std::uint32_t timeout, bool authorized) {
  if (!authorized) return {Code::Forbidden, invalid()};
  if (!timeout || timeout > 0x7fffffffU || r.isSent() || r.isPaused() || r.getResponse() ||
      r.disconnected203() || !r.client()) return {Code::Invalid, invalid()};
  auto keep = r.shared_from_this();
  if (!keep) return {Code::Invalid, invalid()};
  for (std::size_t i = 0; i < Capacity; ++i) {
   if (!slots_[i].occupied) continue;
   auto live = slots_[i].request.lock();
   if (live && live.get() == &r) return {Code::Conflict, invalid()};
  }
  std::size_t index = Capacity;
  for (std::size_t i = 0; i < Capacity; ++i) {
   Slot &s = slots_[i];
   if ((!s.occupied || (s.disconnected && s.request.expired())) &&
       s.generation != std::numeric_limits<std::uint64_t>::max()) { index = i; break; }
  }
  if (index == Capacity) return {Code::Full, invalid()};

  const auto opened = snapshots_.open(services_.now(), timeout);
  if (opened.code == download207::Code::Full) return {Code::Full, invalid()};
  if (opened.code == download207::Code::Invalid) return {Code::Invalid, invalid()};
  if (opened.code != download207::Code::Accepted) return {Code::Unavailable, invalid()};

  Slot &s = slots_[index];
  const std::size_t length = snapshots_.remaining(opened.handle);
  AsyncWebServerResponse *response = services_.makeDownload(s.reader, length);
  if (!response) { snapshots_.close(opened.handle); return {Code::Unavailable, invalid()}; }
  if (!r.installDeferred203(poll, gone, &s)) {
   delete response;
   snapshots_.close(opened.handle);
   return {Code::Conflict, invalid()};
  }
  s.response = response;
  s.snap = opened.handle;
  s.request = keep;
  ++s.generation;
  s.started = services_.now();
  s.timeout = timeout;
  s.occupied = true;
  s.disconnected = false;
  s.done = false;
  s.sent = false;
  s.failed = false;
  return {Code::Accepted, {index, s.generation}};
 }

 // Hands the response to the request. The caller then runs the normal _send().
 bool finish(Handle h, AsyncWebServerRequest &r) {
  Slot *s = find(h, r);
  if (!s || s->done || s->sent || !s->response) return false;
  if (r.isSent() || r.getResponse() || r.isPaused()) { release(*s); return false; }
  AsyncWebServerResponse *response = s->response;
  s->response = nullptr;
  s->sent = true;  // ownership moves to the request before send can fail
  r.send(response);
  return true;
 }

 bool released(Handle h) const { return h.slot < Capacity && (!slots_[h.slot].occupied || slots_[h.slot].generation != h.generation); }
 std::size_t pinned() const {
  std::size_t n = 0;
  for (const auto &s : slots_) if (s.occupied) ++n;
  return n;
 }
 bool full() const { return pinned() == Capacity; }
};
}
