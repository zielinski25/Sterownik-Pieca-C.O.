#include "Arduino.h"
#include "Fixture208.inc"
#include "DownloadAdapter.h"
#include "Snapshots.h"
#include <vector>

using download207::Code;
using download207::ReadStatus;
using Snap = download207::Snapshots<Owner, struct Writer, 2>;

// Test-only writer/rotator implementing the 207 Source contract.
struct Writer {
 Lock &lock_; Owner &owner_; std::size_t committed_ = 0; bool ready_ = true;
 enum class Rotation { Rotated, Deferred, Failed };
 Writer(Lock &l, Owner &o) : lock_(l), owner_(o) {}
 history197::Mutex &mutex() { return lock_; }
 const char *activePath() { return "/history"; }
 std::size_t committed() const { return committed_; }
 bool ready() const { return ready_; }
 bool append(const std::string &line) {
  lock_.lock();
  bool ok = owner_.ready() && ready_;
  fs::File f;
  if (ok) { f = owner_.fs().open("/history", "a", true); ok = static_cast<bool>(f); }
  if (ok) ok = f.seek(static_cast<uint32_t>(f.size()));
  std::size_t n = 0;
  if (ok) n = f.write(reinterpret_cast<const uint8_t *>(line.data()), line.size());
  if (f) f.close();
  if (ok && n == line.size()) committed_ += n; else ok = false;
  lock_.unlock();
  return ok;
 }
 Rotation rotate(Snap &snaps) {
  if (!snaps.beginRotation()) return Rotation::Deferred;
  lock_.lock();
  bool ok = owner_.ready() && ready_;
  if (ok) ok = owner_.fs().rename("/history", "/history.old");
  if (ok) ok = owner_.fs().remove("/history.old");
  if (ok) { fs::File f = owner_.fs().open("/history", "w", true); ok = static_cast<bool>(f); if (f) f.close(); }
  if (ok) committed_ = 0;
  lock_.unlock();
  snaps.endRotation();
  return ok ? Rotation::Rotated : Rotation::Failed;
 }
};

struct Services {
 std::uint32_t clock = 0; bool oom = false; unsigned made = 0; std::function<void()> onMake;
 std::uint32_t now() { return clock; }
 AsyncWebServerResponse *makeDownload(download208::Reader &reader, std::size_t length) {
  if (onMake) onMake();
  if (oom) return nullptr;
  ++made;
  return new download208::Response(reader, length);
 }
};

using Adapter = download208::Adapter<Snap, struct Services>;
struct Bench {
 Model model; Lock fs, hist; Backend backend;
 history197::Permanent<Owner> os; Owner &o;
 Writer writer;
 history197::Permanent<Snap> ss; Snap &snaps;
 Services services;
 history197::Permanent<Adapter> as; Adapter &a;
 Bench() : backend(model), o(os.construct(backend, fs)), writer(hist, o), snaps(ss.construct(o, writer)),
           a(as.construct(snaps, services)) { assert(o.enable()); }
};
static Bench *volatile roots[1024];
static unsigned used = 0;
static Bench &fresh() { assert(used < 1024); Bench *b = new Bench; roots[used++] = b; return *b; }

struct Session {
 Bench &b;
 std::shared_ptr<AsyncWebServerRequest> req;
 Client *client = nullptr;
 std::string wire;
 explicit Session(Bench &x) : b(x) {}
 void open() {
  client = new Client;
  req = AsyncWebServerRequest::create(client);
 }
 bool admit(std::uint32_t timeout = 1000) {
  auto r = b.a.begin(*req, timeout, true);
  code = r.code;
  if (r.code == Adapter::Code::Accepted) handle = r.handle;  // keep the last admitted handle
  return r.code == Adapter::Code::Accepted;
 }
 void finish() {
  if (!b.a.finish(handle, *req)) { std::cerr << "finish failed slot=" << handle.slot << " gen=" << handle.generation << "\n"; std::abort(); }
  req->_send();
  unacked = client->wire.size();  // bytes handed to the socket by _send() are acked later
 }
 // Models TCP progress the way AsyncTCP does it: lwIP only calls the poll hook once
 // the socket has nothing in flight, and every byte handed to the socket comes back
 // as an ack carrying its real length. One network event per round, so the library's
 // in-flight credits are never drained by two competing writes in the same round.
 bool pump(unsigned rounds = 2000) {
  for (unsigned i = 0; i < rounds; ++i) {
   client->window = 4096;                // the peer keeps granting a fresh receive window
   const std::size_t before = client->wire.size();
   if (unacked) {
    const std::size_t sent = unacked; unacked = 0;
    req->_onAck(sent, 0);               // real TCP ack path closes a finished response
   } else {
    req->_onPoll();                      // deferred hook, then the real send loop
   }
   if (client->closed) return true;
   unacked = client->wire.size() - before;
  }
  return client->closed;
 }
 void drain() { while (!req->disconnected203() && client->closed == false) { if (!pump(10)) break; } if (!req->disconnected203()) client->close(); req.reset(); }
 Adapter::Handle handle{};
 Adapter::Code code{};
 std::size_t unacked = 0;
};

// The real 199 loop writes "HEAD" (modeled header bytes) followed by content.
static std::string payload(const std::string &wire) {
 assert(wire.size() >= 4 && wire.compare(0, 4, "HEAD") == 0);
 return wire.substr(4);
}

int main() {
 unsigned groups = 0;
 // 1. 200 with the pinned bytes and the exact Content-Length.
 { auto &b = fresh(); assert(b.writer.append("A1\n") && b.writer.append("A2\n"));
   Session s(b); s.open(); assert(s.admit()); s.finish();
   assert(s.req->getResponse()->announced() == 6);
   assert(s.pump()); assert(payload(s.client->wire) == "A1\nA2\n"); assert(s.b.snaps.pinned() == 0);
   s.drain(); ++groups; }
 // 2. Appends during send never extend the download.
 { auto &b = fresh(); assert(b.writer.append("OLD\n"));
   Session s(b); s.open(); assert(s.admit()); s.finish();
   assert(b.writer.append("OLD2\n") && b.writer.append("OLD3\n"));
   assert(s.pump()); assert(payload(s.client->wire) == "OLD\n"); ++groups; }
 // 3. Rotation is deferred while the download is pinned, then succeeds after close.
 { auto &b = fresh(); assert(b.writer.append("row\n"));
   Session s(b); s.open(); assert(s.admit()); s.finish();
   assert(b.writer.rotate(b.snaps) == Writer::Rotation::Deferred);
   assert(s.pump()); assert(s.b.snaps.pinned() == 0);
   assert(b.writer.rotate(b.snaps) == Writer::Rotation::Rotated);
   assert(b.model.files["/history"].empty()); s.drain(); ++groups; }
 // 4. Two concurrent downloads with independent pins; third is refused.
 { auto &b = fresh(); assert(b.writer.append("one\n"));
   Session x(b), y(b), z(b); x.open(); y.open(); z.open();
   assert(x.admit() && y.admit()); assert(!z.admit() && z.code == Adapter::Code::Full);
   x.finish(); y.finish();
   assert(b.writer.append("two\n"));
   assert(x.pump() && y.pump());
   assert(payload(x.client->wire) == "one\n"); assert(payload(y.client->wire) == "one\n");
   assert(b.snaps.pinned() == 0); x.drain(); y.drain(); z.drain(); ++groups; }
 // 5. Capacity 2: after both end, the pool is reusable with a new generation.
 { auto &b = fresh(); assert(b.writer.append("x\n"));
   Session x(b), y(b); x.open(); y.open(); assert(x.admit() && y.admit());
   x.finish(); y.finish();
   const auto old = x.handle; assert(x.pump() && y.pump()); x.drain(); y.drain();
   Session z(b); z.open(); assert(z.admit()); z.finish(); assert(z.handle.generation != old.generation || z.handle.slot != old.slot);
   assert(z.pump()); assert(payload(z.client->wire) == "x\n"); z.drain(); ++groups; }
 // 6. Disconnect in the middle frees the pin without writing anything further.
 { auto &b = fresh(); const std::string big(3000, 'z'); assert(b.writer.append(big));
   Session s(b); s.open(); assert(s.admit()); s.client->limit = 5; s.finish();
   s.client->window = 4096;
   s.req->_onPoll(); assert(!s.client->closed);
   s.client->close(); assert(s.b.snaps.pinned() == 0);
   assert(b.writer.rotate(b.snaps) == Writer::Rotation::Rotated);
   s.drain(); ++groups; }
 // 7. Deadline: no progress, then abort and release; rotation is possible again.
 { auto &b = fresh(); assert(b.writer.append("slow\n"));
   Session s(b); s.open(); assert(s.admit(10)); s.client->window = 0; s.finish();
   b.services.clock = 5; s.req->_onPoll(); assert(!s.client->closed);
   b.services.clock = 9; s.req->_onPoll(); assert(!s.client->closed);
   b.services.clock = 10; s.req->_onPoll();
   // abort() after _send() is a documented no-op in the pinned library; the
   // released pin makes the source invalid, so the send loop closes instead.
   assert(s.client->closed && s.b.snaps.pinned() == 0);
   assert(s.req->getResponse()->state() == RESPONSE_FAILED); s.drain(); ++groups; }
 // 8. Returned clock wrap keeps the deadline arithmetic correct.
 { auto &b = fresh(); assert(b.writer.append("wrap\n")); b.services.clock = 0xfffffff0u;
   Session s(b); s.open(); assert(s.admit(32)); s.client->window = 0; s.finish();
   b.services.clock = 15; s.req->_onPoll(); assert(!s.client->closed);
   b.services.clock = 16; s.req->_onPoll(); assert(s.client->closed); s.drain(); ++groups; }
 // 9. Storage stops mid-download: source invalid, client closed, never a clean end.
 { auto &b = fresh(); const std::string big(2000, 'q'); assert(b.writer.append(big));
   Session s(b); s.open(); assert(s.admit()); s.client->limit = 3; s.finish();
   s.req->_onPoll();
   b.o.trip();
   s.client->window = 4096; s.client->limit = 4096; s.req->_onPoll();
   assert(s.client->closed); assert(s.b.snaps.pinned() == 0);
   assert(payload(s.client->wire).size() < 2000); s.drain(); ++groups; }
 // 10. Storage already stopped: admission refused, no response allocated.
 { auto &b = fresh(); assert(b.writer.append("x\n")); b.o.trip();
   Session s(b); s.open(); assert(!s.admit()); assert(b.services.made == 0); s.drain(); ++groups; }
 // 11. Response allocation failure releases the pin immediately.
 { auto &b = fresh(); assert(b.writer.append("x\n")); b.services.oom = true;
   Session s(b); s.open(); assert(!s.admit() && !b.a.full()); assert(b.snaps.pinned() == 0);
   assert(b.writer.rotate(b.snaps) == Writer::Rotation::Rotated); s.drain(); ++groups; }
 // 12. Duplicate admission for the same request is a conflict, not a second pin.
 { auto &b = fresh(); assert(b.writer.append("x\n")); Session s(b); s.open(); assert(s.admit());
   assert(!s.admit() && s.code == Adapter::Code::Conflict);
   assert(b.snaps.pinned() == 1); s.finish(); assert(s.pump()); s.drain(); ++groups; }
 // 13. Unauthorized admission never touches the snapshots.
 { auto &b = fresh(); assert(b.writer.append("x\n")); Session s(b); s.open();
   const auto r = b.a.begin(*s.req, 100, false);
   assert(r.code == Adapter::Code::Forbidden && b.snaps.pinned() == 0);
   assert(b.services.made == 0); s.drain(); ++groups; }
 // 14. Invalid arguments: zero/oversized timeout, sent and paused requests.
 { auto &b = fresh(); assert(b.writer.append("x\n"));
   Session s(b); s.open();
   assert(b.a.begin(*s.req, 0, true).code == Adapter::Code::Invalid);
   assert(b.a.begin(*s.req, 0x80000000u, true).code == Adapter::Code::Invalid);
   s.req->pause();
   assert(b.a.begin(*s.req, 100, true).code == Adapter::Code::Invalid);
   s.req->send(404);
   assert(b.a.begin(*s.req, 100, true).code == Adapter::Code::Invalid);
   assert(b.snaps.pinned() == 0); s.drain(); ++groups; }
 // 15. finish twice, finish a released handle or another request's handle is refused.
 { auto &b = fresh(); assert(b.writer.append("x\n")); Session x(b), y(b); x.open(); y.open();
   assert(x.admit()); assert(!b.a.finish(x.handle, *y.req));
   assert(b.a.finish(x.handle, *x.req)); x.req->_send(); assert(!b.a.finish(x.handle, *x.req));
   assert(x.pump()); x.drain(); y.drain(); ++groups; }
 // 16. Empty history is a valid zero-length download.
 { auto &b = fresh(); assert(b.writer.append("")); Session s(b); s.open(); assert(s.admit()); s.finish();
   assert(s.req->getResponse()->announced() == 0);
   assert(s.pump()); assert(payload(s.client->wire).empty()); s.drain(); ++groups; }
 // 17. Fifty sequential downloads reuse the pool; every one sees its own prefix.
 { auto &b = fresh();
   for (unsigned i = 0; i < 50; ++i) {
    assert(b.writer.append("n" + std::to_string(i) + "\n"));
    Session s(b); s.open(); assert(s.admit()); s.finish(); assert(s.pump());
    std::string want; for (unsigned j = 0; j <= i; ++j) want += "n" + std::to_string(j) + "\n";
    assert(payload(s.client->wire) == want); s.drain();
   }
   assert(b.snaps.pinned() == 0 && !b.a.full()); ++groups; }
 // 18. Small TCP windows with partial acceptance still deliver the exact bytes.
 { auto &b = fresh(); const std::string body(200, 'k'); assert(b.writer.append(body));
   for (std::size_t limit : {std::size_t(1), std::size_t(7), std::size_t(30)}) {
    Session s(b); s.open(); assert(s.admit()); s.client->limit = limit; s.finish();
    assert(s.pump(4000)); assert(payload(s.client->wire) == body); s.drain();
   } ++groups; }
 // 19. A late ack after the response finished cannot reopen a released pin.
 { auto &b = fresh(); assert(b.writer.append("done\n"));
   Session s(b); s.open(); assert(s.admit()); s.finish(); assert(s.pump());
   const std::size_t wire = s.client->wire.size(); assert(b.snaps.pinned() == 0);
   s.req->_onAck(0, 0);
   assert(s.client->closed); assert(s.client->wire.size() == wire); s.drain(); ++groups; }
 // 20. After rotation a new download sees only the new file.
 { auto &b = fresh(); assert(b.writer.append("old\n"));
   Session s(b); s.open(); assert(s.admit()); s.finish(); assert(s.pump()); s.drain();
   assert(b.writer.rotate(b.snaps) == Writer::Rotation::Rotated);
   assert(b.writer.append("new\n"));
   Session t(b); t.open(); assert(t.admit()); t.finish(); assert(t.pump());
   assert(payload(t.client->wire) == "new\n"); t.drain(); ++groups; }
 // 21. A short write is a storage fault: committed length unchanged, service refused.
 { auto &b = fresh(); assert(b.writer.append("keep\n")); b.model.partial = true;
   assert(!b.writer.append("lost\n")); b.model.partial = false; assert(b.writer.committed() == 5);
   assert(b.o.fault() != history197::Fault::None);
   Session s(b); s.open(); assert(!s.admit()); assert(b.snaps.pinned() == 0); s.drain(); ++groups; }
 std::cout << "PASS download208 groups=" << groups << " benches=" << used
           << "; real 199 send loop + 203 hooks + 207 pin + 197/FS.cpp; modeled client, not ESP\n";
 return 0;
}
