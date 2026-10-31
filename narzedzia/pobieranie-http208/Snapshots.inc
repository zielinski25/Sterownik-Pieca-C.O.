#pragma once
#include <FS.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <limits>
#include "RuntimeFilesystem.h"

namespace download207 {
enum class Code { Accepted, Full, Unavailable, Conflict, Stale, Invalid };
// Data: bytes returned. End: exactly the pinned length was delivered.
// Stopped: storage/owner no longer usable. Invalid: caller misuse.
// Stale: handle expired, closed, reused or foreign.
enum class ReadStatus { Data, End, Stopped, Invalid, Stale };
struct ReadResult {
 ReadStatus status; std::size_t bytes;
 ReadResult(ReadStatus s, std::size_t n = 0) : status(s), bytes(n) {}
};

// Network context only. One instance per history file; permanent lifetime.
//
// Source contract (implemented by the history writer/rotator, same owner):
//   history197::Mutex& mutex();          the SAME mutex append and rotation take
//   const char* activePath();            stable string, no open needed to read it
//   std::size_t committed();             confirmed committed length of the active file
//   bool ready();                        callable without that mutex
//
// Lock order (as in 198): source mutex -> Owner::Lease. This class never calls
// back into Source while holding the Owner lease.
//
// A pinned download blocks rotation. Appends may continue: only the pinned
// prefix is ever returned. Nothing here rewrites, renames or deletes the file.
template <class Owner, class Source, std::size_t Capacity = 2, std::size_t PathBytes = 64>
class Snapshots {
 static_assert(Capacity > 0 && Capacity < 256, "bounded pool");
 static_assert(PathBytes > 1, "path capacity");
public:
 struct Handle { std::size_t slot; std::uint64_t generation; };
 struct Begin { Code code; Handle handle; };

private:
 struct Slot {
  fs::File file;
  std::uint64_t generation = 0;
  std::size_t length = 0, sent = 0;
  std::uint32_t started = 0, timeout = 0;
  char path[PathBytes] = {};
  bool occupied = false;
 };
 Owner& owner_;
 Source& source_;
 Slot slots_[Capacity];
 bool rotating_ = false;

 static Handle invalid() { return {Capacity, 0}; }
 Slot* find(Handle h) {
  if (h.slot >= Capacity || !h.generation) return nullptr;
  Slot& s = slots_[h.slot];
  return (s.occupied && s.generation == h.generation) ? &s : nullptr;
 }
 void release(Slot& s) {
  s.file = fs::File();
  s.occupied = false;
  s.length = 0;
  s.sent = 0;
  s.started = 0;
  s.timeout = 0;
 }
 static bool expired(const Slot& s, std::uint32_t now) {
  return s.timeout && static_cast<std::uint32_t>(now - s.started) >= s.timeout;
 }
 static bool copyPath(Slot& s, const char* p) {
  if (!p) return false;
  std::size_t n = 0;
  while (n < PathBytes && p[n]) ++n;
  if (n == PathBytes) return false;
  std::memcpy(s.path, p, n + 1);
  return true;
 }

public:
 Snapshots(Owner& o, Source& s) : owner_(o), source_(s) {}
 ~Snapshots() = delete;
 Snapshots(const Snapshots&) = delete;
 Snapshots& operator=(const Snapshots&) = delete;

 // Takes a consistent prefix under the source mutex, then opens the file once.
 Begin open(std::uint32_t now, std::uint32_t timeout) {
  if (!timeout || timeout > 0x7fffffffU || rotating_) return {Code::Unavailable, invalid()};
  if (!owner_.ready() || !source_.ready()) return {Code::Unavailable, invalid()};
  std::size_t index = Capacity;
  for (std::size_t i = 0; i < Capacity; ++i) {
   if (!slots_[i].occupied) { index = i; break; }
  }
  if (index == Capacity) return {Code::Full, invalid()};
  const char* path = nullptr;
  std::size_t length = 0;
  bool consistent = false;
  {
   history197::Mutex& m = source_.mutex();
   m.lock();
   path = source_.activePath();
   length = source_.committed();
   consistent = source_.ready();
   m.unlock();
  }
  if (!consistent || !path) return {Code::Unavailable, invalid()};
  Slot& s = slots_[index];
  if (!copyPath(s, path)) return {Code::Invalid, invalid()};
  if (s.generation == std::numeric_limits<std::uint64_t>::max()) return {Code::Unavailable, invalid()};
  s.file = owner_.fs().open(s.path, "r", false);
  if (!s.file) { s.file = fs::File(); return {Code::Unavailable, invalid()}; }
  if (!owner_.ready() || !source_.ready()) { s.file = fs::File(); return {Code::Unavailable, invalid()}; }
  // A committed prefix longer than the file means the writer is inconsistent.
  if (s.file.size() < length) { s.file = fs::File(); return {Code::Invalid, invalid()}; }
  ++s.generation;
  s.occupied = true;
  s.length = length;
  s.sent = 0;
  s.started = now;
  s.timeout = timeout;
  return {Code::Accepted, {index, s.generation}};
 }

 ReadResult read(Handle h, unsigned char* buffer, std::size_t capacity, std::uint32_t now) {
  Slot* s = find(h);
  if (!s) return ReadResult(ReadStatus::Stale);
  if (!owner_.ready() || !source_.ready()) { release(*s); return ReadResult(ReadStatus::Stopped); }
  if (expired(*s, now)) { release(*s); return ReadResult(ReadStatus::Stale); }
  if (!buffer || !capacity) return ReadResult(ReadStatus::Invalid);
  if (s->sent == s->length) return ReadResult(ReadStatus::End);
  const std::size_t wanted = std::min(capacity, s->length - s->sent);
  const std::size_t got = s->file.read(buffer, wanted);
  // A stop during the read is not a successful end, and a bad count is not data.
  if (!owner_.ready() || !source_.ready()) { release(*s); return ReadResult(ReadStatus::Stopped); }
  if (got == 0 || got > wanted) { release(*s); return ReadResult(ReadStatus::Stopped); }
  s->sent += got;
  return ReadResult(ReadStatus::Data, got);
 }

 // Idempotent. Unknown or already released handles return false.
 bool close(Handle h) {
  Slot* s = find(h);
  if (!s) return false;
  release(*s);
  return true;
 }

 // Sweeps pins whose deadline passed, so a hung client cannot block rotation.
 std::size_t expire(std::uint32_t now) {
  std::size_t n = 0;
  for (auto& s : slots_) {
   if (s.occupied && expired(s, now)) { release(s); ++n; }
  }
  return n;
 }

 // Rotation must stop new snapshots and wait until every admitted download ends.
 bool beginRotation() {
  if (rotating_ || pinned()) return false;
  rotating_ = true;
  return true;
 }
 void endRotation() { rotating_ = false; }
 bool rotating() const { return rotating_; }
 std::size_t pinned() const {
  std::size_t n = 0;
  for (const auto& s : slots_) if (s.occupied) ++n;
  return n;
 }
 std::size_t remaining(Handle h) const {
  const Slot* s = const_cast<Snapshots*>(this)->find(h);
  return s ? s->length - s->sent : 0;
 }
};
}
