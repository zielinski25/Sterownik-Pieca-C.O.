#include "Arduino.h"
#include "Fixture209.inc"
#include "DownloadAdapter.h"
#include "Snapshots.h"
#include "DownloadRoute.h"
#include <vector>

using Snap = download207::Snapshots<Owner, struct Writer, 2>;
using Adapter = download208::Adapter<Snap, struct Services>;

// Test-only writer/rotator implementing the 207 Source contract.
struct Writer {
 Lock &lock_;
 Owner &owner_;
 std::size_t committed_ = 0;
 bool ready_ = true;
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
 Rotation rotate(Snap &snap) {
  lock_.lock();
  const bool started = snap.beginRotation();
  lock_.unlock();
  if (!started) return Rotation::Deferred;
  fs::File f = owner_.fs().open("/history", "r", false);
  const bool removed = f ? owner_.fs().remove("/history") : false;
  if (f) f.close();
  lock_.lock();
  snap.endRotation();
  const bool done = removed;
  if (done) committed_ = 0;
  lock_.unlock();
  return done ? Rotation::Rotated : Rotation::Failed;
 }
};

struct Services {
 std::uint32_t clock = 0;
 bool oom = false;
 unsigned made = 0;
 std::uint32_t now() { return clock; }
 AsyncWebServerResponse *makeDownload(download208::Reader &reader, std::size_t length) {
  ++made;
  if (oom) return nullptr;
  return new download208::Response(reader, length);
 }
};

using Route = download209::Route<Adapter>;

struct Bench {
 Model model;
 Lock fs_lock, history_lock;
 Backend backend;
 history197::Permanent<Owner> owners;
 Owner &owner;
 Writer writer;
 history197::Permanent<Snap> snaps;
 Snap &snap;
 Services services;
 history197::Permanent<Adapter> adapters;
 Adapter &adapter;
 history197::Permanent<Route> routes;
 Route &route;
 AsyncWebServer server;
 AsyncCallbackWebHandler *handler = nullptr;
 std::unique_ptr<AsyncCallbackWebHandler> catchAll;
 unsigned catchAllCalls = 0;
 std::size_t raw_open_calls = 0;

 Bench(const char *token = "sekret")
   : backend(model),
     owner(owners.construct(backend, fs_lock)),
     writer(history_lock, owner),
     snap(snaps.construct(owner, writer)),
     adapter(adapters.construct(snap, services)),
     route(routes.construct(adapter, token, 1000)) {
  handler = new AsyncCallbackWebHandler;
  handler->setUri(AsyncURIMatcher::exact("/history"));
  handler->setMethod(WebRequestMethodComposite(AsyncWebRequestMethod::HTTP_GET) | AsyncWebRequestMethod::HTTP_POST |
                     AsyncWebRequestMethod::HTTP_PUT | AsyncWebRequestMethod::HTTP_DELETE | AsyncWebRequestMethod::HTTP_HEAD);
  handler->onRequest([this](AsyncWebServerRequest *r) { route.handle(*r); });
  server.addHandler(handler);
  catchAll.reset(new AsyncCallbackWebHandler);
  catchAll->onRequest([this](AsyncWebServerRequest *r) { ++catchAllCalls; r->send(404); });
  server._catchAllHandler = catchAll.get();
  owner.enable();
 }
};

static Bench *volatile roots[256]{};
static unsigned used = 0;
static Bench &fresh(const char *token = "sekret") {
 assert(used < 256);
 Bench *b = new Bench(token);
 roots[used++] = b;
 return *b;
}

struct Exchange {
 std::shared_ptr<AsyncWebServerRequest> req;
 Client *client = nullptr;
};
static Exchange open(Bench &b) {
 Client *c = new Client;
 return {AsyncWebServerRequest::create(&b.server, c), c};
}
static void feed(Exchange &e, const std::string &raw, std::size_t block = 0) {
 if (!block) block = raw.size();
 for (std::size_t i = 0; i < raw.size() && !e.req->disconnected203(); i += block) {
  std::string part = raw.substr(i, block);
  e.req->_onData(&part[0], part.size());
 }
}
// One network event per round, like lwIP: ack what is in flight, else poll the loop.
static bool pump(Exchange &e, unsigned rounds = 4000) {
 std::size_t unacked = e.client->wire.size();
 for (unsigned i = 0; i < rounds; ++i) {
  e.client->window = 4096;
  const std::size_t before = e.client->wire.size();
  if (unacked) {
   const std::size_t sent = unacked;
   unacked = 0;
   e.req->_onAck(sent, 0);
  } else {
   e.req->_onPoll();
  }
  if (e.client->closed) return true;
  unacked = e.client->wire.size() - before;
 }
 return e.client->closed;
}
static void close(Exchange &e) {
 if (e.req && !e.req->disconnected203() && !e.client->closed) e.client->close();
 e.req.reset();
}
static std::string payload(const std::string &wire) {
 assert(wire.size() >= 4 && wire.compare(0, 4, "HEAD") == 0);
 return wire.substr(4);
}
static std::string request(const std::string &method = "GET", const std::string &path = "/history",
                           const std::string &auth = "Authorization: Bearer sekret\r\n") {
 return method + " " + path + " HTTP/1.1\r\nHost: oven\r\n" + auth + "\r\n";
}
static int status(const Exchange &e) {
 return e.req->getResponse() ? e.req->getResponse()->status() : -1;
}

int main() {
 unsigned groups = 0;
 // 1. Real request line and headers, split at every byte, deliver the pinned prefix.
 {
  Bench &b = fresh();
  assert(b.writer.append("A1\n") && b.writer.append("A2\n"));
  const std::string raw = request();
  unsigned expected = 0;
  for (std::size_t cut = 1; cut < raw.size(); ++cut) {
   Exchange e = open(b);
   const bool headersDone = cut >= raw.size() - 2;
   feed(e, raw.substr(0, cut), 1);
   if (!headersDone) assert(b.snap.pinned() == 0);
   feed(e, raw.substr(cut), 1);
   ++expected;
   assert(status(e) == 200 && b.route.stats().downloads == expected);
   assert(pump(e));
   assert(payload(e.client->wire) == "A1\nA2\n");
   assert(b.snap.pinned() == 0);
   close(e);
  }
  ++groups;
 }
 // 2. Another path is handled by the catch-all: 404, no pin, no response from the route.
 {
  Bench &b = fresh();
  assert(b.writer.append("A1\n"));
  Exchange e = open(b);
  feed(e, request("GET", "/historyx"));
  assert(b.catchAllCalls == 1 && status(e) == 404);
  assert(b.adapter.pinned() == 0 && b.route.stats().downloads == 0 && b.services.made == 0);
  assert(pump(e));
  close(e);
  ++groups;
 }
 // 3. The real matcher is exact: a trailing slash is not the same route.
 {
  Bench &b = fresh();
  assert(b.writer.append("A1\n"));
  Exchange e = open(b);
  feed(e, request("GET", "/history/"));
  assert(b.catchAllCalls == 1 && status(e) == 404 && b.adapter.pinned() == 0);
  assert(pump(e));
  close(e);
  ++groups;
 }
 // 4. Wrong method reaches the handler and is refused with 405, nothing is pinned.
 {
  Bench &b = fresh();
  assert(b.writer.append("A1\n"));
  for (const char *m : {"POST", "PUT", "DELETE", "HEAD"}) {
   Exchange e = open(b);
   feed(e, request(m));
   assert(status(e) == 405 && b.route.stats().method >= 1);
   assert(b.adapter.pinned() == 0 && b.services.made == 0);
   assert(pump(e));
   close(e);
  }
  assert(b.route.stats().downloads == 0);
  ++groups;
 }
 // 5. Credentials: missing, wrong, other scheme, empty and case-changed are refused.
 {
  Bench &b = fresh();
  assert(b.writer.append("A1\n"));
  const char *headers[] = {"", "Authorization: Bearer nope\r\n", "Authorization: Basic sekret\r\n",
                          "Authorization: sekret\r\n", "Authorization: Bearer \r\n", "Authorization: Bearer Sekret\r\n",
                          "Authorization: Bearer sekreX\r\n", "Authorization: Bearer sekretx\r\n"};
  for (const char *h : headers) {
   Exchange e = open(b);
   feed(e, request("GET", "/history", h));
   assert(status(e) == 401);
   assert(b.adapter.pinned() == 0 && b.services.made == 0);
   assert(b.model.files.size() == 4 && b.model.files["/history"] == "A1\n");
   assert(pump(e));
   close(e);
  }
  assert(b.route.stats().unauthorized == 8 && b.route.stats().downloads == 0);
  ++groups;
 }
 // 6. Header name case and OWS are handled by the pinned parser, not by the route.
 {
  Bench &b = fresh();
  assert(b.writer.append("ok\n"));
  Exchange e = open(b);
  feed(e, "GET /history HTTP/1.1\r\nhOsT:\toven\r\naUtHoRiZaTiOn:   Bearer sekret   \r\n\r\n", 2);
  assert(status(e) == 200);
  assert(pump(e));
  assert(payload(e.client->wire) == "ok\n");
  close(e);
  ++groups;
 }
 // 7. Query string is stripped by the real parser; the route still matches the path.
 {
  Bench &b = fresh();
  assert(b.writer.append("q\n"));
  Exchange e = open(b);
  feed(e, request("GET", "/history?x=1&y=2"));
  assert(e.req->url() == "/history" && e.req->contentLength() == 0);
  assert(status(e) == 200);
  assert(pump(e));
  assert(payload(e.client->wire) == "q\n");
  close(e);
  ++groups;
 }
 // 8. Percent-encoded path is decoded before matching: /hist%6Fry is the same route.
 {
  Bench &b = fresh();
  assert(b.writer.append("p\n"));
  Exchange e = open(b);
  feed(e, request("GET", "/hist%6Fry"));
  assert(e.req->url() == "/history" && status(e) == 200);
  assert(pump(e));
  assert(payload(e.client->wire) == "p\n");
  close(e);
  ++groups;
 }
 // 9. Appends during the download never extend it; the length is the pinned one.
 {
  Bench &b = fresh();
  assert(b.writer.append("one\n"));
  Exchange e = open(b);
  feed(e, request());
  assert(status(e) == 200);
  assert(b.writer.append("two\n") && b.writer.append("three\n"));
  assert(pump(e));
  assert(payload(e.client->wire) == "one\n");
  assert(b.snap.pinned() == 0);
  close(e);
  ++groups;
 }
 // 10. Two real requests share the pool; the third gets 503 and leaves no pin.
 {
  Bench &b = fresh();
  assert(b.writer.append("x\n"));
  Exchange a = open(b), c = open(b), d = open(b);
  feed(a, request());
  feed(c, request());
  feed(d, request());
  assert(status(a) == 200 && status(c) == 200);
  assert(status(d) == 503 && b.route.stats().busy == 1);
  assert(b.adapter.pinned() == 2);
  assert(pump(a) && pump(c));
  assert(payload(a.client->wire) == "x\n" && payload(c.client->wire) == "x\n");
  assert(b.snap.pinned() == 0);
  assert(pump(d));
  close(a);
  close(c);
  close(d);
  Exchange e = open(b);
  feed(e, request());
  assert(status(e) == 200);
  assert(pump(e));
  assert(payload(e.client->wire) == "x\n");
  close(e);
  ++groups;
 }
 // 11. A client that disappears mid-download releases the pin; rotation can proceed.
 {
  Bench &b = fresh();
  const std::string big(3000, 'z');
  assert(b.writer.append(big));
  Exchange e = open(b);
  e.client->limit = 5;   // the peer accepts a few bytes per write
  feed(e, request());
  e.client->window = 4096;
  e.req->_onPoll();
  assert(!e.client->closed && e.client->wire.size() < 100);
  e.client->close();
  assert(b.snap.pinned() == 0);
  assert(b.writer.rotate(b.snap) == Writer::Rotation::Rotated);
  e.req.reset();
  ++groups;
 }
 // 12. Storage stops mid-download: the source becomes invalid, never a clean end.
 {
  Bench &b = fresh();
  const std::string big(2000, 'q');
  assert(b.writer.append(big));
  Exchange e = open(b);
  e.client->limit = 3;
  feed(e, request());
  e.req->_onPoll();
  b.owner.trip();
  e.client->window = 4096;
  e.client->limit = 4096;
  e.req->_onPoll();
  assert(e.client->closed);
  assert(e.req->getResponse()->state() == RESPONSE_FAILED);
  assert(payload(e.client->wire).size() < 2000);
  assert(b.snap.pinned() == 0);
  close(e);
  ++groups;
 }
 // 13. Storage already stopped: 503 and no response allocated by the service.
 {
  Bench &b = fresh();
  assert(b.writer.append("x\n"));
  b.owner.trip();
  Exchange e = open(b);
  feed(e, request());
  assert(status(e) == 503 && b.route.stats().unavailable == 1);
  assert(b.services.made == 0 && b.adapter.pinned() == 0);
  assert(pump(e));
  close(e);
  ++groups;
 }
 // 14. Allocation failure is 503 (the adapter cannot tell it from storage), pin released.
 {
  Bench &b = fresh();
  assert(b.writer.append("x\n"));
  b.services.oom = true;
  Exchange e = open(b);
  feed(e, request());
  assert(status(e) == 503 && b.adapter.pinned() == 0 && b.services.made == 1);
  assert(pump(e));
  close(e);
  b.services.oom = false;
  Exchange f = open(b);
  feed(f, request());
  assert(status(f) == 200);
  assert(pump(f));
  assert(payload(f.client->wire) == "x\n");
  close(f);
  ++groups;
 }
 // 15. Deadline on a stalled peer: connection closed, pin released, not a 200.
 {
  Bench &b = fresh();
  assert(b.writer.append("slow\n"));
  Exchange e = open(b);
  e.client->window = 0;
  feed(e, request());
  assert(status(e) == 200);
  b.services.clock = 999;
  e.req->_onPoll();
  assert(!e.client->closed);
  b.services.clock = 1000;
  e.req->_onPoll();
  assert(e.client->closed && b.snap.pinned() == 0);
  assert(e.req->getResponse()->state() == RESPONSE_FAILED);
  close(e);
  ++groups;
 }
 // 16. A late ack after the response finished cannot reopen a released pin.
 {
  Bench &b = fresh();
  assert(b.writer.append("done\n"));
  Exchange e = open(b);
  feed(e, request());
  assert(pump(e));
  const std::size_t wire = e.client->wire.size();
  assert(b.snap.pinned() == 0);
  e.req->_onAck(0, 0);
  assert(e.client->wire.size() == wire && e.client->closed);
  close(e);
  ++groups;
 }
 // 17. Fifty real requests reuse the same pool; each sees its own prefix.
 {
  Bench &b = fresh();
  for (unsigned i = 0; i < 50; ++i) {
   assert(b.writer.append("n" + std::to_string(i) + "\n"));
   Exchange e = open(b);
   feed(e, request());
   assert(status(e) == 200);
   assert(pump(e));
   std::string want;
   for (unsigned j = 0; j <= i; ++j) want += "n" + std::to_string(j) + "\n";
   assert(payload(e.client->wire) == want);
   close(e);
  }
  assert(b.snap.pinned() == 0 && !b.adapter.full());
  assert(b.route.stats().downloads == 50);
  ++groups;
 }
 // 18. Small receive windows: exact bytes are still delivered.
 {
  Bench &b = fresh();
  const std::string body(200, 'k');
  assert(b.writer.append(body));
  for (std::size_t limit : {std::size_t(1), std::size_t(7), std::size_t(30)}) {
   Exchange e = open(b);
   e.client->limit = limit;
   feed(e, request());
   assert(status(e) == 200);
   assert(pump(e));
   assert(payload(e.client->wire) == body);
   close(e);
  }
  ++groups;
 }
 // 19. Empty history is a valid zero-length download.
 {
  Bench &b = fresh();
  assert(b.writer.append(""));
  Exchange e = open(b);
  feed(e, request());
  assert(status(e) == 200 && payload(e.client->wire).empty());
  assert(pump(e));
  assert(payload(e.client->wire).empty());
  close(e);
  ++groups;
 }
 // 20. Clock wrap keeps the deadline arithmetic correct.
 {
  Bench &b = fresh();
  assert(b.writer.append("wrap\n"));
  b.services.clock = 0xfffffff0u;
  Exchange e = open(b);
  e.client->window = 0;
  feed(e, request());
  b.services.clock = 983;   // 0xfffffff0 + 999, no wrap yet
  e.req->_onPoll();
  assert(!e.client->closed && b.snap.pinned() == 1);
  b.services.clock = 984;   // 0xfffffff0 + 1000 elapsed across the wrap
  e.req->_onPoll();
  assert(e.client->closed && b.snap.pinned() == 0);
  close(e);
  ++groups;
 }
 // 21. After rotation a new request sees only the new file.
 {
  Bench &b = fresh();
  assert(b.writer.append("old\n"));
  Exchange e = open(b);
  feed(e, request());
  assert(pump(e));
  assert(payload(e.client->wire) == "old\n");
  close(e);
  assert(b.writer.rotate(b.snap) == Writer::Rotation::Rotated);
  assert(b.writer.append("new\n"));
  Exchange f = open(b);
  feed(f, request());
  assert(pump(f));
  assert(payload(f.client->wire) == "new\n");
  close(f);
  ++groups;
 }
 // 22. Constant-time comparison: equal length, different bytes; prefix and suffix cases.
 {
  using download209::equalConstantTime;
  assert(equalConstantTime("abc", 3, "abc", 3));
  assert(!equalConstantTime("abc", 3, "abd", 3));
  assert(!equalConstantTime("abc", 3, "ab", 2));
  assert(!equalConstantTime("ab", 2, "abc", 3));
  assert(!equalConstantTime("", 0, "a", 1));
  assert(equalConstantTime("", 0, "", 0));
  assert(!equalConstantTime("xabc", 4, "abc", 3));
  Bench &b = fresh("token");
  assert(b.writer.append("t\n"));
  Exchange e = open(b);
  feed(e, request("GET", "/history", "Authorization: Bearer token\r\n"));
  assert(status(e) == 200);
  assert(pump(e));
  close(e);
  ++groups;
 }
 // 23. A request that never reaches the route (aborted request line) leaves no trace.
 {
  Bench &b = fresh();
  assert(b.writer.append("x\n"));
  Exchange e = open(b);
  feed(e, "BREW /history HTTP/1.1\r\n\r\n");
  assert(b.adapter.pinned() == 0);
  assert(b.catchAllCalls == 0 && b.route.stats().downloads == 0 && b.adapter.pinned() == 0);
  e.req.reset();
  ++groups;
 }
 std::cout << "PASS route209 groups=" << groups << " benches=" << used
           << "; real parse + real matcher + real handler selection + real 199 loop + 203 hooks + 207 pin + 208 adapter\n";
 return 0;
}
