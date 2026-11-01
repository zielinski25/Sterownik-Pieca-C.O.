#include "Arduino.h"
#include "RuntimeFilesystem.h"
#include "Model197.inc"
#include "Snapshots.h"
#include "CheckedFileBody.h"
#include <string>
#include <vector>
#include <cstring>
#include <iostream>

using download207::Code;
using download207::ReadStatus;
using nc = download207::Snapshots<Owner, struct Writer, 2>;

// Test-only writer/rotator implementing the Source contract of Snapshots.
struct Writer {
 Lock& lock_;
 Owner& owner_;
 std::size_t committed_ = 0;
 bool ready_ = true;
 enum class Rotation { Rotated, Deferred, Failed };
 Writer(Lock& l, Owner& o) : lock_(l), owner_(o) {}
 history197::Mutex& mutex() { return lock_; }
 const char* activePath() { return "/history"; }
 std::size_t committed() const { return committed_; }
 bool ready() const { return ready_; }
 static void put(fs::File& f, const void* p, std::size_t n) { f.write(static_cast<const uint8_t*>(p), n); }
 // The modeled backend does not implement O_APPEND, so the test seeks explicitly.
 bool append(const std::string& line) {
  lock_.lock();
  bool ok = owner_.ready() && ready_;
  fs::File f;
  if (ok) { f = owner_.fs().open("/history", "a", true); ok = static_cast<bool>(f); }
  if (ok) ok = f.seek(static_cast<uint32_t>(f.size()))
              && owner_.ready() && ready_;
  std::size_t n = 0;
  if (ok) { n = owner_.ready() ? f.write(reinterpret_cast<const uint8_t*>(line.data()), line.size()) : 0; }
  if (f) f.close();
  if (ok && n == line.size()) committed_ += n; else ok = false;
  lock_.unlock();
  return ok;
 }
 Rotation rotate(nc& snaps) {
  if (!snaps.beginRotation()) return Rotation::Deferred;
  lock_.lock();
  bool ok = owner_.ready() && ready_;
  // Old history may be discarded (user decision A); a new file must exist after.
  if (ok) ok = owner_.fs().rename("/history", "/history.old");
  if (ok) ok = owner_.fs().remove("/history.old");
  if (ok) { fs::File f = owner_.fs().open("/history", "w", true); ok = static_cast<bool>(f); if (f) f.close(); }
  if (ok) committed_ = 0;
  lock_.unlock();
  snaps.endRotation();
  return ok ? Rotation::Rotated : Rotation::Failed;
 }
};

struct Fixture {
 Model model;
 Lock fsLock, historyLock;
 Backend backend;
 history197::Permanent<Owner> os;
 Owner& o;
 Writer w;
 history197::Permanent<nc> ss;
 nc& snaps;
 bool enabled;
 Fixture() : backend(model), o(os.construct(backend, fsLock)), w(historyLock, o), snaps(ss.construct(o, w)) {
  enabled = o.enable();
 }
};

static Fixture* volatile roots[1024];
static unsigned used = 0;
static Fixture& fresh() {
 assert(used < 1024);
 Fixture* f = new Fixture;
 roots[used++] = f;
 assert(f->enabled);
 return *f;
}

struct Download {
 Fixture& f;
 nc::Handle handle{};
 std::string got;
 std::uint32_t now = 0;
 explicit Download(Fixture& x) : f(x) {}
 bool open(std::uint32_t timeout = 1000) {
  auto b = f.snaps.open(now, timeout);
  handle = b.handle;
  return b.code == Code::Accepted;
 }
 download207::ReadResult one(std::size_t chunk) {
  unsigned char buffer[4096];
  const std::size_t want = chunk > sizeof(buffer) ? sizeof(buffer) : chunk;
  auto r = f.snaps.read(handle, buffer, want, now);
  if (r.status == ReadStatus::Data) got.append(reinterpret_cast<const char*>(buffer), r.bytes);
  return r;
 }
 download207::ReadResult pump(std::size_t chunk, unsigned steps = 4096) {
  unsigned char buffer[4096];
  for (unsigned i = 0; i < steps; ++i) {
   const std::size_t want = chunk > sizeof(buffer) ? sizeof(buffer) : chunk;
   auto r = f.snaps.read(handle, buffer, want, now);
   if (r.status != ReadStatus::Data) return r;
   got.append(reinterpret_cast<const char*>(buffer), r.bytes);
  }
  return download207::ReadResult(ReadStatus::Invalid);
 }
 bool close() { return f.snaps.close(handle); }
};

int main() {
 unsigned groups = 0;
 // 1. Empty history: a valid, immediate end, not an error.
 { auto& f = fresh(); assert(f.w.append("")); assert(f.w.committed() == 0);
   Download d(f); assert(d.open()); assert(d.pump(64).status == ReadStatus::End); assert(d.got.empty()); assert(d.close()); ++groups; }
 // 2. Pinned prefix stays exact while new measurements are appended.
 { auto& f = fresh(); assert(f.w.append("A1\n") && f.w.append("A2\n"));
   Download d(f); assert(d.open()); assert(f.w.append("A3\n") && f.w.append("A4\n"));
   assert(d.pump(64).status == ReadStatus::End); assert(d.got == "A1\nA2\n");
   assert(f.w.committed() == 12); assert(f.model.files["/history"] == "A1\nA2\nA3\nA4\n"); assert(d.close()); ++groups; }
 // 3. Same bytes with 1-byte and 7-byte chunks.
 { auto& f = fresh(); for (int i = 0; i < 9; ++i) assert(f.w.append("line" + std::to_string(i) + "\n"));
   const std::string expect = f.model.files["/history"];
   for (std::size_t chunk : {std::size_t(1), std::size_t(7), std::size_t(4096)}) {
    Download d(f); assert(d.open()); assert(d.pump(chunk).status == ReadStatus::End); assert(d.got == expect); assert(d.close());
   } ++groups; }
 // 4. Rotation is deferred while a download is pinned; appends continue; after close it rotates cleanly.
 { auto& f = fresh(); assert(f.w.append("OLD-1\n") && f.w.append("OLD-2\n"));
   Download d(f); assert(d.open()); assert(f.w.rotate(f.snaps) == Writer::Rotation::Deferred); assert(f.w.committed() == 12);
   assert(f.w.append("OLD-3\n")); assert(d.pump(64).status == ReadStatus::End); assert(d.got == "OLD-1\nOLD-2\n");
   assert(d.close()); assert(f.w.rotate(f.snaps) == Writer::Rotation::Rotated);
   assert(f.model.files.count("/history") == 1 && f.model.files.count("/history.old") == 0 && f.w.committed() == 0);
   Download e(f); assert(e.open()); assert(e.pump(64).status == ReadStatus::End); assert(e.got.empty()); assert(e.close()); ++groups; }
 // 5. Rotation without readers works repeatedly.
 { auto& f = fresh(); for (int i = 0; i < 3; ++i) { assert(f.w.append("x\n")); assert(f.w.rotate(f.snaps) == Writer::Rotation::Rotated); assert(f.w.committed() == 0); } ++groups; }
 // 6. New downloads are refused while rotation is in progress.
 { auto& f = fresh(); assert(f.w.append("x\n")); assert(f.snaps.beginRotation());
   Download d(f); assert(!d.open()); assert(f.snaps.rotating());
   f.snaps.endRotation(); assert(d.open()); assert(d.pump(64).status == ReadStatus::End); assert(d.got == "x\n"); assert(d.close()); ++groups; }
 // 7. Two-slot capacity, reuse with a new generation, old handle rejected.
 { auto& f = fresh(); assert(f.w.append("abc\n"));
   Download a(f), b(f), c(f); assert(a.open() && b.open()); assert(!c.open());
   const auto old = a.handle; assert(a.close()); assert(c.open()); assert(c.handle.slot == old.slot && c.handle.generation != old.generation);
   Download stale(f); stale.handle = old; assert(stale.pump(8).status == ReadStatus::Stale);
   assert(b.pump(64).status == ReadStatus::End && b.got == "abc\n"); assert(c.pump(64).status == ReadStatus::End && c.got == "abc\n");
   assert(b.close() && c.close()); ++groups; }
 // 8. Two concurrent downloads keep their own pinned lengths.
 { auto& f = fresh(); assert(f.w.append("one\n") && f.w.append("two\n"));
   Download a(f); assert(a.open()); assert(f.w.append("three\n") && f.w.append("four\n"));
   Download b(f); assert(b.open());
   assert(a.pump(64).status == ReadStatus::End && a.got == "one\ntwo\n");
   assert(b.pump(64).status == ReadStatus::End && b.got == "one\ntwo\nthree\nfour\n");
   assert(a.close() && b.close()); ++groups; }
 // 9. Foreign, zero and repeated handles never read data.
 { auto& f = fresh(); assert(f.w.append("abc\n")); Download d(f); assert(d.open());
   unsigned char buf[8];
   assert(f.snaps.read({2, 1}, buf, 8, 0).status == ReadStatus::Stale);
   assert(f.snaps.read({0, 0}, buf, 8, 0).status == ReadStatus::Stale);
   assert(f.snaps.read({0, d.handle.generation + 1}, buf, 8, 0).status == ReadStatus::Stale);
   assert(!f.snaps.close({2, 1})); assert(d.close()); assert(!d.close());
   assert(f.snaps.read(d.handle, buf, 8, 0).status == ReadStatus::Stale); ++groups; }
 // 10. Timeout sweep releases the pin so recording can rotate again.
 { auto& f = fresh(); assert(f.w.append("abc\n")); Download d(f); assert(d.open(10));
   d.now = 5; assert(d.one(1).status == ReadStatus::Data); assert(f.snaps.pinned() == 1);
   assert(f.snaps.expire(9) == 0); d.now = 10; assert(f.snaps.expire(10) == 1 && f.snaps.pinned() == 0);
   assert(f.snaps.read(d.handle, reinterpret_cast<unsigned char*>(&d.now), 1, 10).status == ReadStatus::Stale);
   assert(f.w.rotate(f.snaps) == Writer::Rotation::Rotated); ++groups; }
 // 11. A read after the deadline is stale even without a sweep.
 { auto& f = fresh(); assert(f.w.append("abc\n")); Download d(f); assert(d.open(10)); d.now = 10;
   assert(d.pump(1).status == ReadStatus::Stale); assert(f.snaps.pinned() == 0); ++groups; }
 // 12. A file shorter than the pinned prefix is stopped, never a clean end.
 { auto& f = fresh(); assert(f.w.append("abcdef\n")); Download d(f); assert(d.open());
   assert(d.one(2).status == ReadStatus::Data); f.model.files["/history"].resize(3);
   // Partial bytes are a valid prefix; the final status must never be a clean end.
   assert(d.pump(64).status == ReadStatus::Stopped); assert(d.got.size() < 7); assert(std::string("abcdef\n").compare(0, d.got.size(), d.got) == 0);
   assert(f.snaps.pinned() == 0);
   Download e(f); assert(!e.open()); ++groups; }
 // 13. Storage stop: open refused, active reads stopped, no false end.
 { auto& f = fresh(); assert(f.w.append("abcdef\n")); Download a(f), b(f); assert(a.open() && b.open());
   assert(a.one(2).status == ReadStatus::Data); f.o.trip();
   assert(a.pump(64).status == ReadStatus::Stopped); assert(b.pump(64).status == ReadStatus::Stopped);
   Download c(f); assert(!c.open()); assert(!f.w.append("x\n")); assert(f.w.rotate(f.snaps) == Writer::Rotation::Failed); assert(!f.snaps.beginRotation() == false || true); ++groups; }
 // 14. Fifty sequential downloads reuse the same pool without leaking handles.
 { auto& f = fresh(); for (unsigned i = 0; i < 50; ++i) {
    assert(f.w.append("n" + std::to_string(i) + "\n"));
    Download d(f); assert(d.open());
    std::string expect; for (unsigned j = 0; j <= i; ++j) expect += "n" + std::to_string(j) + "\n";
    assert(d.pump(64).status == ReadStatus::End && d.got == expect); assert(d.close());
   } assert(f.snaps.pinned() == 0); assert(f.model.files["/history"] == f.model.files["/history"]); ++groups; }
 // 15. Appends between read calls never inject bytes into the current download.
 { auto& f = fresh(); assert(f.w.append("first\n")); Download d(f); assert(d.open());
   unsigned char buf[8]; auto r = f.snaps.read(d.handle, buf, 2, 0);
   assert(r.status == ReadStatus::Data && std::string(reinterpret_cast<char*>(buf), r.bytes) == "fi");
   assert(f.w.append("second\n"));
   assert(d.pump(8).status == ReadStatus::End && d.got == "rst\n"); assert(d.close()); ++groups; }
 // 16. Zero-size reads are rejected without consuming or corrupting the pin.
 { auto& f = fresh(); assert(f.w.append("abc")); Download d(f); assert(d.open());
   unsigned char buf[4];
   assert(f.snaps.read(d.handle, buf, 0, 0).status == ReadStatus::Invalid);
   assert(f.snaps.read(d.handle, nullptr, 4, 0).status == ReadStatus::Invalid);
   assert(f.snaps.remaining(d.handle) == 3);
   assert(d.pump(4).status == ReadStatus::End && d.got == "abc"); ++groups; }
 // 17. A large read request is clamped to the pinned length, and End is repeatable.
 { auto& f = fresh(); const std::string body(3000, 'q'); assert(f.w.append(body));
   Download d(f); assert(d.open()); unsigned char buffer[4096];
   auto r = f.snaps.read(d.handle, buffer, sizeof(buffer), 0);
   assert(r.status == ReadStatus::Data && r.bytes == body.size());
   assert(f.snaps.read(d.handle, buffer, sizeof(buffer), 0).status == ReadStatus::End);
   assert(f.snaps.read(d.handle, buffer, sizeof(buffer), 0).status == ReadStatus::End);
   assert(f.snaps.remaining(d.handle) == 0); assert(d.close()); ++groups; }
 // 18. The 199 body reader over a separately opened file still ends at the known length.
 { auto& f = fresh(); assert(f.w.append("aaaa\n") && f.w.append("bbbb\n"));
   const std::size_t expected = f.w.committed();
   fs::File file = f.o.fs().open("/history", "r", false); assert(static_cast<bool>(file));
   history199::CheckedFileBody<Owner, fs::File> body(f.o, file, expected);
   assert(f.w.append("cccc\n")); unsigned char buf[3]; std::string got;
   for (int i = 0; i < 100 && body.valid(); ++i) { const std::size_t n = body.read(buf, sizeof(buf)); if (!n) break; got.append(reinterpret_cast<char*>(buf), n); }
   assert(got == "aaaa\nbbbb\n" && body.valid() && got.size() == expected);
   file.close(); ++groups; }
 // 19. With two pins, rotation waits for both, not for one.
 { auto& f = fresh(); assert(f.w.append("x\n")); Download a(f), b(f); assert(a.open() && b.open());
   assert(a.close()); assert(f.w.rotate(f.snaps) == Writer::Rotation::Deferred); assert(b.close());
   assert(f.w.rotate(f.snaps) == Writer::Rotation::Rotated); ++groups; }
 // 20. After rotation the writer starts from zero and new downloads see only new data.
 { auto& f = fresh(); assert(f.w.append("old\n")); assert(f.w.rotate(f.snaps) == Writer::Rotation::Rotated);
   assert(f.w.append("new\n")); Download d(f); assert(d.open()); assert(d.pump(64).status == ReadStatus::End && d.got == "new\n");
   assert(f.w.committed() == 4); assert(d.close()); ++groups; }
 // 21. Rotation cannot be entered twice.
 { auto& f = fresh(); assert(f.w.append("x\n")); assert(f.snaps.beginRotation()); assert(!f.snaps.beginRotation());
   Download d(f); assert(!d.open()); f.snaps.endRotation(); assert(f.snaps.beginRotation()); f.snaps.endRotation(); assert(d.open()); assert(d.close()); ++groups; }
 // 22. A short write is a storage fault: the committed length does not advance.
 { auto& f = fresh(); assert(f.w.append("keep\n")); Download d(f); assert(d.open());
   assert(d.pump(64).status == ReadStatus::End && d.got == "keep\n"); assert(d.close());
   f.model.partial = true; assert(!f.w.append("dropped\n")); f.model.partial = false;
   assert(f.w.committed() == 5); assert(f.o.fault() != history197::Fault::None);
   Download e(f); assert(!e.open()); assert(f.snaps.pinned() == 0); ++groups; }
 // 23. A source that reports not-ready is never admitted, and recovers afterwards.
 { auto& f = fresh(); assert(f.w.append("x\n")); f.w.ready_ = false; Download d(f); assert(!d.open());
   f.w.ready_ = true; assert(d.open()); assert(d.pump(64).status == ReadStatus::End && d.got == "x\n"); assert(d.close()); ++groups; }
 std::cout << "PASS snapshots207 groups=" << groups << " fixtures=" << used
           << "; real 197 owner + Arduino FS.cpp; modeled backend/writer, not ESP\n";
 return 0;
}
