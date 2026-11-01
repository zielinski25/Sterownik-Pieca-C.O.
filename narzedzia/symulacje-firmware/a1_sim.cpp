// Symulacja startu i czyszczenia historii na komputerze (bez ESP32).
//
// Po co: firmware ma gotowy, audytowany kanal jednorazowego czyszczenia przy
// starcie (history_a190::Transition + history_a191::Storage + history195::Boot).
// Ten program BASICOWO odtwarza te klasy 1:1 z repo (include prawdziwych
// naglowkow) na pamieciowym systemie plikow, a potem przepuszcza przez nie
// scenariusze: normalne czyszczenie, zanik pradu w kazdym kroku, uszkodzone
// zadanie, bledy I/O, powtorki. Do tego model bramek zapisu (przelacznik).
//
// Uruchomienie:  ./run-all.sh   (albo: g++ -std=gnu++17 -O1 -o /tmp/a1 a1_sim.cpp && /tmp/a1)
//
// Czego ten program NIE robi: nie kompiluje main_centrala.cpp (wymaga Arduino.h
// i ESP-IDF). Funkcje firmware'u (historiaFsCzyscZadaj, bramki) sa tu
// PRZEPISANE krok po kroku; zgodnosc przepisu ze zrodlem pilnuje test statyczny
// narzedzia/testy-firmware/test-hist-fs.js.

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

#include "HistoryStartup.h"   // history193::Calls, history194::Startup
#include "Storage.h"          // history_a190::*, history_a191::Calls/Storage
#include "HistoryBoot.h"      // history195::Boot

// ═══════════════════════════════════════════════════════════════════════════
// 1. Pamieciowy system plikow + dwa adaptery (takie same interfejsy jak w repo)
// ═══════════════════════════════════════════════════════════════════════════
namespace sim {

const char* const kShort = "/littlefs/hist_short.csv";
const char* const kLong  = "/littlefs/hist_long.csv";
const char* const kReq   = "/littlefs/history_reset_a1.req";
const char* const kReqTmp= "/littlefs/history_reset_a1.req.tmp";
const char* const kReqBad= "/littlefs/history_reset_a1.req.bad";
const char* const kDone  = "/littlefs/history_reset_a1.done";
const char* const kRebS  = "/littlefs/hist_short.csv.rebuild";
const char* const kRebL  = "/littlefs/hist_long.csv.rebuild";

const char kZadanie[] = "RESET-HISTORY-A1\n";
const size_t kZadanieLen = sizeof(kZadanie) - 1;   // 17
const char kComplete[] = "HISTORY-A1-COMPLETE\n";

struct Vfs {
  std::map<std::string, std::string> pliki;
  struct Fd { std::string p; size_t pos = 0; };
  std::map<int, Fd> fds;
  int nextFd = 7;
  int err = 0;

  // wstrzykiwanie awarii sprzetu
  std::set<std::string> zleOtwarcie, zlyZapis, zleUsuniecie, zlySync, zlyRename, zleSkrocenie;
  bool failRemoveDone = false;

  bool exists(const std::string& p) const { return pliki.count(p) != 0; }
  size_t size(const std::string& p) const {
    auto it = pliki.find(p); return it == pliki.end() ? 0 : it->second.size();
  }
  std::string readAll(const std::string& p) const {
    auto it = pliki.find(p); return it == pliki.end() ? std::string() : it->second;
  }
  void writeAll(const std::string& p, const std::string& d) { pliki[p] = d; }

  // „tryb w": skroc + zaloz
  bool createTruncate(const std::string& p) {
    if (zleOtwarcie.count(p)) { err = EIO; return false; }
    if (zleSkrocenie.count(p)) { err = EIO; return false; }
    pliki[p] = "";
    return true;
  }
  size_t writeAt(const std::string& p, size_t off, const char* d, size_t n) {
    if (zlyZapis.count(p)) { err = EIO; return 0; }
    std::string& f = pliki[p];
    if (off + n > f.size()) f.resize(off + n, '\0');
    f.replace(off, n, std::string(d, n));
    return n;
  }
  bool remove(const std::string& p) {
    if (zleUsuniecie.count(p) || (failRemoveDone && p == kDone)) { err = EIO; return false; }
    auto it = pliki.find(p);
    if (it == pliki.end()) { err = ENOENT; return false; }
    pliki.erase(it);
    return true;
  }
  bool rename(const std::string& a, const std::string& b) {
    if (zlyRename.count(b)) { err = EIO; return false; }
    auto it = pliki.find(a);
    if (it == pliki.end()) { err = ENOENT; return false; }
    pliki[b] = it->second;
    pliki.erase(it);
    return true;
  }
};

// ── adapter 1: history193::Calls (uchwyty numeryczne, jak POSIX w EspVfsCalls)
struct Calls193 final : history193::Calls {
  Vfs& v;
  explicit Calls193(Vfs& vfs) : v(vfs) {}
  int open(const char* p, int flags) override {
    v.err = 0;
    std::string path(p);
    const bool jest = v.exists(path);
    if (!jest && !(flags & O_CREAT)) { v.err = ENOENT; return -1; }
    if (v.zleOtwarcie.count(path)) { v.err = EIO; return -1; }
    if (!jest) v.pliki[path] = "";
    const int fd = v.nextFd++;
    v.fds[fd] = Vfs::Fd{path, 0};
    return fd;
  }
  int stat(int fd, struct stat* s) override {
    v.err = 0;
    auto it = v.fds.find(fd);
    if (it == v.fds.end()) { v.err = EBADF; return -1; }
    std::memset(s, 0, sizeof(*s));
    s->st_mode = S_IFREG | 0644;
    s->st_size = static_cast<off_t>(v.size(it->second.p));
    return 0;
  }
  off_t seek(int fd, off_t pos) override {
    v.err = 0;
    auto it = v.fds.find(fd);
    if (it == v.fds.end() || pos < 0) { v.err = EBADF; return -1; }
    it->second.pos = static_cast<size_t>(pos);
    return pos;
  }
  ssize_t read(int fd, void* b, std::size_t n) override {
    v.err = 0;
    auto it = v.fds.find(fd);
    if (it == v.fds.end()) { v.err = EBADF; return -1; }
    const std::string& d = v.pliki[it->second.p];
    if (it->second.pos >= d.size()) return 0;
    const size_t ile = std::min(n, d.size() - it->second.pos);
    std::memcpy(b, d.data() + it->second.pos, ile);
    it->second.pos += ile;
    return static_cast<ssize_t>(ile);
  }
  ssize_t write(int fd, const void* b, std::size_t n) override {
    v.err = 0;
    auto it = v.fds.find(fd);
    if (it == v.fds.end()) { v.err = EBADF; return -1; }
    const size_t ile = v.writeAt(it->second.p, it->second.pos, static_cast<const char*>(b), n);
    if (!ile) return -1;
    it->second.pos += ile;
    return static_cast<ssize_t>(ile);
  }
  int sync(int fd) override {
    v.err = 0;
    auto it = v.fds.find(fd);
    if (it == v.fds.end()) { v.err = EBADF; return -1; }
    if (v.zlySync.count(it->second.p)) { v.err = EIO; return -1; }
    return 0;
  }
  int close(int fd) override {
    v.err = 0;
    if (!v.fds.count(fd)) { v.err = EBADF; return -1; }
    v.fds.erase(fd);
    return 0;
  }
  int rename(const char* a, const char* b) override { v.err = 0; return v.rename(a, b) ? 0 : -1; }
  int error() const override { return v.err; }
};

// ── adapter 2: history_a191::Calls (jeden uchwyt, Path enum) — jak EspVfsCalls
struct CallsA1 final : history_a191::Calls {
  Vfs& v;
  std::string otwarty;
  bool jestOtwarty = false;
  bool exclusive = false;
  explicit CallsA1(Vfs& vfs) : v(vfs) {}
  static const char* nazwa(history_a191::Path p) {
    using P = history_a191::Path;
    switch (p) {
      case P::Request: return kReq;
      case P::Completion: return kDone;
      case P::Short: return kShort;
      case P::Long: return kLong;
    }
    return "";
  }
  int stat(history_a191::Path p) override {
    v.err = 0;
    const std::string n = nazwa(p);
    if (v.zleUsuniecie.count(n) && v.failRemoveDone) { v.err = EIO; return -1; }
    return v.exists(n) ? 0 : -2;
  }
  int open(history_a191::Path p, history_a191::Open mode) override {
    using O = history_a191::Open;
    v.err = 0;
    const std::string n = nazwa(p);
    if (jestOtwarty) { v.err = EBADF; return -1; }
    if (v.zleOtwarcie.count(n)) { v.err = EIO; return -1; }
    if (mode == O::ExclusiveCreate) {
      if (v.exists(n)) { v.err = EEXIST; return -1; }
      v.pliki[n] = "";
    } else if (mode == O::ExistingReadWrite) {
      if (!v.exists(n)) { v.err = ENOENT; return -2; }
      v.fds[100] = Vfs::Fd{n, 0};
    } else {
      if (!v.exists(n)) { v.err = ENOENT; return -2; }
      v.fds[100] = Vfs::Fd{n, 0};
    }
    otwarty = n;
    jestOtwarty = true;
    exclusive = (mode == O::ExclusiveCreate);
    if (exclusive) v.fds[100] = Vfs::Fd{n, 0};
    return 0;
  }
  int read(void* b, std::size_t n) override {
    v.err = 0;
    if (!jestOtwarty) { v.err = EBADF; return -1; }
    Vfs::Fd& f = v.fds[100];
    const std::string& d = v.pliki[otwarty];
    if (f.pos >= d.size()) return 0;
    const size_t ile = std::min(n, d.size() - f.pos);
    std::memcpy(b, d.data() + f.pos, ile);
    f.pos += ile;
    return static_cast<int>(ile);
  }
  int write(const void* b, std::size_t n) override {
    v.err = 0;
    if (!jestOtwarty) { v.err = EBADF; return -1; }
    Vfs::Fd& f = v.fds[100];
    const size_t ile = v.writeAt(otwarty, f.pos, static_cast<const char*>(b), n);
    if (!ile) return -1;
    f.pos += ile;
    return static_cast<int>(ile);
  }
  bool rewind() override {
    v.err = 0;
    if (!jestOtwarty) return false;
    v.fds[100].pos = 0;
    return true;
  }
  bool sync() override {
    v.err = 0;
    if (!jestOtwarty) return false;
    if (v.zlySync.count(otwarty)) { v.err = EIO; return false; }
    return true;
  }
  bool close() override {
    v.err = 0;
    if (!jestOtwarty) return false;
    jestOtwarty = false;
    v.fds.erase(100);
    return true;
  }
  bool remove(history_a191::Path p) override {
    v.err = 0;
    return v.remove(nazwa(p));
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// 2. Odpowiedniki funkcji z firmware (przepisane 1:1)
// ═══════════════════════════════════════════════════════════════════════════

// historiaFsCzyscZadaj() z 3.57.6: .done usun, tmp zapisz, sprawdz, rename.
enum Krok { K_BRAK = 0, K_PO_DONE = 1, K_PO_TMP = 2, K_W_TRAKCIE = 3, K_PO_ODCZYT = 4, K_PRZED_RENAME = 5 };

bool czyscZadaj(Vfs& v, int awaria = K_BRAK) {
  if (v.exists(kDone) && !v.remove(kDone)) return false;      // 1
  if (awaria == K_PO_DONE) return false;
  if (v.exists(kReqTmp)) v.remove(kReqTmp);
  if (!v.createTruncate(kReqTmp)) return false;               // 2
  if (awaria == K_PO_TMP) return false;
  // Zanik pradu w polowie zapisu: na dysku zostaje 9 z 17 bajtow.
  const size_t n = (awaria == K_W_TRAKCIE) ? v.writeAt(kReqTmp, 0, kZadanie, 9)
                                           : v.writeAt(kReqTmp, 0, kZadanie, kZadanieLen);
  if (awaria == K_W_TRAKCIE) return false;
  if (n != kZadanieLen) { v.remove(kReqTmp); return false; }
  if (awaria == K_PO_ODCZYT) return false;
  const std::string tmp = v.readAll(kReqTmp);                 // 3 odczyt kontrolny
  if (tmp != std::string(kZadanie, kZadanieLen)) { v.remove(kReqTmp); return false; }
  if (awaria == K_PRZED_RENAME) return false;
  if (!v.rename(kReqTmp, kReq)) { v.remove(kReqTmp); return false; }  // 4
  return true;
}

// Wersja 3.57.5 (do pokazania, co bylo nie tak): zapis BEZPOSREDNIO do .req
// trybem "w" = najpierw skrocenie do zera, potem 17 bajtow.
bool czyscZadaj3575(Vfs& v, bool awariaWTrakcieZapisu, bool usunDone) {
  if (usunDone && v.exists(kDone) && !v.remove(kDone)) return false;
  if (!v.createTruncate(kReq)) return false;
  if (awariaWTrakcieZapisu) { v.writeAt(kReq, 0, kZadanie, 9); return false; }
  return v.writeAt(kReq, 0, kZadanie, kZadanieLen) == kZadanieLen;
}

// historiaFsNaprawUszkodzoneZadanie() z 3.57.6.
bool naprawUszkodzoneZadanie(Vfs& v) {
  if (!v.exists(kReq)) return false;
  const std::string d = v.readAll(kReq);
  if (d == std::string(kZadanie, kZadanieLen)) return false;   // poprawne - nie ruszamy
  if (v.exists(kReqBad)) v.remove(kReqBad);
  if (!v.rename(kReq, kReqBad)) v.remove(kReq);
  return true;
}

struct WynikBootu {
  bool ok = false;
  bool startupReady = false;
  history_a190::Result a1 = history_a190::Result::Blocked;
  history_a190::Reason powod = history_a190::Reason::None;
  history193::Fault fault = history193::Fault::None;
  uint64_t shortBytes = 0, longBytes = 0;
};

WynikBootu boot(Vfs& v, bool mounted = true) {
  Calls193 h193(v);
  CallsA1 a1(v);
  history194::Startup startup(h193);
  history_a191::Storage store(a1);
  history195::Boot b(h193, store, startup);
  history194::Sizes s;
  WynikBootu w;
  w.ok = b.start(mounted, s);
  w.startupReady = startup.ready();
  w.a1 = b.transition().result;
  w.powod = b.transition().reason;
  w.shortBytes = s.shortBytes;
  w.longBytes = s.longBytes;
  return w;
}

}  // namespace sim

// ═══════════════════════════════════════════════════════════════════════════
// 3. Scenariusze
// ═══════════════════════════════════════════════════════════════════════════
using namespace sim;

static int ok = 0, fail = 0;
static void check(const char* nazwa, bool warunek, const std::string& detal = "") {
  if (warunek) { ok++; std::printf("  OK    %s\n", nazwa); }
  else { fail++; std::printf("  FAIL  %s%s%s\n", nazwa, detal.empty() ? "" : " — ", detal.c_str()); }
}

static void swiezy(Vfs& v) {
  v = Vfs{};
  v.pliki[kShort] = "10:00,1,2,3\n10:05,1,2,3\n10:10,1,2,3\n";
  v.pliki[kLong]  = "10:00,1,2,3,4\n";
}

int main() {
  std::printf("=== SYMULACJA PROTOKOLU A1 + BRAMEK ZAPISU (3.57.6) ===\n");
  std::printf("zadanie: '%s' = %zu B, potwierdzenie: '%s' = %zu B\n\n",
              kZadanie, kZadanieLen, kComplete, std::strlen(kComplete));

  // ── S1: zwykly start, brak znacznikow
  std::printf("[S1] Zwykly start, brak znacznikow\n");
  {
    Vfs v; swiezy(v);
    auto w = boot(v);
    check("start OK, Startup Ready", w.ok && w.startupReady);
    check("A1 = Existing (nic nie kasuje)", w.a1 == history_a190::Result::Existing);
    check("oba CSV nietkniete", v.exists(kShort) && v.exists(kLong));
    check("rozmiary z dysku przeliczone", w.shortBytes == v.size(kShort) && w.longBytes == v.size(kLong));
    check("brak .done i .req", !v.exists(kDone) && !v.exists(kReq));
  }

  // ── S2: pelny cykl: komenda -> restart -> pliki usuniete
  std::printf("\n[S2] Komenda czyszczenia -> restart\n");
  {
    Vfs v; swiezy(v);
    check("czyscZadaj() zwraca true", czyscZadaj(v));
    check("po komendzie .done nie ma, .req jest 17 B",
          !v.exists(kDone) && v.readAll(kReq) == std::string(kZadanie, kZadanieLen));
    check("pliki CSV jeszcze sa (kasowanie dopiero przy starcie)", v.exists(kShort) && v.exists(kLong));
    auto w = boot(v);
    check("start OK, Startup Ready", w.ok && w.startupReady);
    check("A1 = Fresh (skasowalo)", w.a1 == history_a190::Result::Fresh);
    check("oba CSV usuniete", !v.exists(kShort) && !v.exists(kLong));
    check(".done zapisane z poprawna trescia", v.readAll(kDone) == std::string(kComplete, strlen(kComplete)));
    check(".req zjedzone, .tmp nie zostal", !v.exists(kReq) && !v.exists(kReqTmp));
    check("rozmiary w statusie = 0", w.shortBytes == 0 && w.longBytes == 0);
  }

  // ── S3: bez usuniecia .done (tak by zrobilo 3.57.4/5 przy drugim czyszczeniu)
  std::printf("\n[S3] Zadanie bez usuniecia .done (stary blad) — pliki MUSZA zostac\n");
  {
    Vfs v; swiezy(v);
    v.pliki[kDone] = kComplete;              // po poprzednim czyszczeniu
    v.pliki[kReq] = kZadanie;                // ktos zlozyl samo zadanie
    auto w = boot(v);
    check("start OK", w.ok && w.startupReady);
    check("A1 = Fresh (sciezka release)", w.a1 == history_a190::Result::Fresh);
    check("pliki CSV NIE zostaly usuniete", v.exists(kShort) && v.exists(kLong));
    check(".req zjedzone", !v.exists(kReq));
    check("=> dlatego 3.57.6 usuwa .done PRZED zlozeniem zadania", true);
  }

  // ── S4: 5 cykli pod rzad
  std::printf("\n[S4] Piec cykli czyszczenia pod rzad\n");
  {
    Vfs v; swiezy(v);
    bool wszystkie = true;
    for (int i = 0; i < 5; ++i) {
      wszystkie = wszystkie && czyscZadaj(v);
      v.pliki[kShort] = "11:00,1,2,3\n";     // po restarcie znowu cos sie zapisze
      v.pliki[kLong] = "11:00,1,2,3\n";
      wszystkie = wszystkie && boot(v).a1 == history_a190::Result::Fresh;
      wszystkie = wszystkie && !v.exists(kShort) && !v.exists(kLong) && v.exists(kDone);
    }
    check("kazdy cykl kasuje oba pliki i zostawia .done", wszystkie);
    check("po 5 cyklach brak .req/.tmp", !v.exists(kReq) && !v.exists(kReqTmp));
  }

  // ── S5..S7: zanik pradu w kazdym kroku komendy
  std::printf("\n[S5] Zanik pradu po usunieciu .done, przed zapisem\n");
  {
    Vfs v; swiezy(v);
    v.pliki[kDone] = kComplete;
    czyscZadaj(v, K_PO_DONE);
    auto w = boot(v);
    check("start OK, historia dziala", w.ok && w.startupReady);
    check("pliki nietkniete (nie bylo zadania)", v.exists(kShort) && v.exists(kLong));
    check("brak .req", !v.exists(kReq));
  }

  std::printf("\n[S6] Zanik pradu W TRAKCIE zapisu pliku tymczasowego\n");
  {
    Vfs v; swiezy(v);
    czyscZadaj(v, K_W_TRAKCIE);
    check("powstal tylko kawalek .tmp (9 z 17 B)", v.exists(kReqTmp) && v.readAll(kReqTmp).size() == 9);
    auto w = boot(v);
    check("start OK, historia dziala", w.ok && w.startupReady);
    check("pliki nietkniete", v.exists(kShort) && v.exists(kLong));
    check(".req nie powstal (to jest poprawka 3.57.6)", !v.exists(kReq));
    check("nastepna komenda sprzata .tmp", czyscZadaj(v) && !v.exists(kReqTmp));
  }

  std::printf("\n[S7] Zanik pradu po zapisie .tmp, przed rename\n");
  {
    Vfs v; swiezy(v);
    czyscZadaj(v, K_PRZED_RENAME);
    check(".tmp ma pelne 17 B", v.readAll(kReqTmp) == std::string(kZadanie, kZadanieLen));
    check(".req nie powstal", !v.exists(kReq));
    auto w = boot(v);
    check("start OK, historia dziala", w.ok && w.startupReady);
    check("pliki nietkniete", v.exists(kShort) && v.exists(kLong));
    check("nastepna komenda konczy sprawe", czyscZadaj(v) && boot(v).a1 == history_a190::Result::Fresh);
  }

  // ── S8: co by bylo w 3.57.5 (zapis bezposredni) + naprawa z 3.57.6
  std::printf("\n[S8] 3.57.5: zanik pradu przy zapisie BEZPOSREDNIM do .req\n");
  {
    Vfs v; swiezy(v);
    czyscZadaj3575(v, true, true);           // 9 z 17 bajtow w .req
    check(".req istnieje, ale ma tylko 9 B", v.exists(kReq) && v.readAll(kReq).size() == 9);
    auto w = boot(v);
    check("Transition BLOKUJE (RequestRead)", w.a1 == history_a190::Result::Blocked &&
          w.powod == history_a190::Reason::RequestRead);
    check("Startup NIE wstal — historia nie dziala", !w.startupReady && !w.ok);
    check("pliki CSV zostaly (nic nie skasowane)", v.exists(kShort) && v.exists(kLong));
    check("=> bez naprawy ten stan trwalby na zawsze", true);

    check("naprawa przenosi uszkodzone zadanie", naprawUszkodzoneZadanie(v));
    check(".req.bad istnieje, .req nie ma", v.exists(kReqBad) && !v.exists(kReq));
    auto w2 = boot(v);
    check("po naprawie historia startuje", w2.ok && w2.startupReady);
    check("pliki CSV nietkniete", v.exists(kShort) && v.exists(kLong));
  }

  // ── S9: naprawa nie rusza poprawnego zadania
  std::printf("\n[S9] Naprawa przy poprawnym zadaniu\n");
  {
    Vfs v; swiezy(v);
    v.pliki[kReq] = kZadanie;
    check("naprawa nic nie robi", naprawUszkodzoneZadanie(v) == false && v.exists(kReq));
    auto w = boot(v);
    check("zadanie wykonane, pliki usuniete", w.a1 == history_a190::Result::Fresh &&
          !v.exists(kShort) && !v.exists(kLong));
  }

  // ── S10: I/O przy usuwaniu .done
  std::printf("\n[S10] Blad I/O przy usuwaniu .done\n");
  {
    Vfs v; swiezy(v);
    v.pliki[kDone] = kComplete;
    v.zleUsuniecie.insert(kDone);
    check("czyscZadaj() zwraca false", czyscZadaj(v) == false);
    check("zadanie NIE zostalo zlozone", !v.exists(kReq) && !v.exists(kReqTmp));
    auto w = boot(v);
    check("po restarcie nic sie nie dzieje, historia zyje", w.ok && w.startupReady &&
          v.exists(kShort) && v.exists(kLong));
  }

  // ── S11: nie da sie zalozyc pliku tymczasowego
  std::printf("\n[S11] Brak miejsca / blad przy zakladaniu .tmp\n");
  {
    Vfs v; swiezy(v);
    v.zleOtwarcie.insert(kReqTmp);
    check("czyscZadaj() zwraca false", czyscZadaj(v) == false);
    check("brak .req i .tmp", !v.exists(kReq) && !v.exists(kReqTmp));
    check("pliki CSV nietkniete", v.exists(kShort) && v.exists(kLong));
  }

  // ── S12: rename sie nie udaje
  std::printf("\n[S12] Blad rename (zadanie sie nie publikuje)\n");
  {
    Vfs v; swiezy(v);
    v.zlyRename.insert(kReq);
    check("czyscZadaj() zwraca false", czyscZadaj(v) == false);
    check(".tmp posprzatany, brak .req", !v.exists(kReqTmp) && !v.exists(kReq));
    auto w = boot(v);
    check("historia startuje normalnie", w.ok && w.startupReady);
  }

  // ── S13: .req poprawny, ale .done tez jest
  std::printf("\n[S13] Poprawne zadanie przy istniejacym .done\n");
  {
    Vfs v; swiezy(v);
    v.pliki[kDone] = kComplete;
    v.pliki[kReq] = kZadanie;
    auto w = boot(v);
    check("A1 = Fresh, ale to sciezka release", w.a1 == history_a190::Result::Fresh);
    check("pliki NIE usuniete", v.exists(kShort) && v.exists(kLong));
    check(".req zjedzone, .done przepieczetowane", !v.exists(kReq) &&
          v.readAll(kDone) == std::string(kComplete, strlen(kComplete)));
  }

  // ── S14: istnieje tylko krotki plik
  std::printf("\n[S14] Istnieje tylko hist_short.csv\n");
  {
    Vfs v; swiezy(v);
    v.remove(kLong);
    check("czyscZadaj(), potem start", czyscZadaj(v) && boot(v).a1 == history_a190::Result::Fresh);
    check("krotki usuniety, dlugi i tak nie istnial", !v.exists(kShort) && !v.exists(kLong));
    check(".done zapisane", v.exists(kDone));
  }

  // ── S15: brak zamontowanego FS
  std::printf("\n[S15] LittleFS nie zamontowany\n");
  {
    Vfs v; swiezy(v);
    auto w = boot(v, false);
    check("historia nie startuje", !w.ok && !w.startupReady);
    check("nic nie skasowane", v.exists(kShort) && v.exists(kLong));
    check("brak .done", !v.exists(kDone));
  }

  // ── S16: plik bez konczacego '\n' (istniejace zachowanie, nie nasze)
  std::printf("\n[S16] CSV uciety — ostatni bajt to nie '\\n'\n");
  {
    Vfs v; swiezy(v);
    v.pliki[kShort] = "10:00,1,2,3";
    auto w = boot(v);
    check("Startup odrzuca plik (Boundary), historia nie startuje",
          !w.startupReady && !w.ok);
    check("nic nie skasowane (fail-closed, bez formatowania)",
          v.exists(kShort) && v.exists(kLong));
  }

  // ── S17: plik .rebuild blokuje caly kanal
  std::printf("\n[S17] Istnieje hist_short.csv.rebuild\n");
  {
    Vfs v; swiezy(v);
    v.pliki[kRebS] = "x";
    auto w = boot(v);
    check("start zatrzymany, brak zmian", !w.ok && v.exists(kShort) && !v.exists(kDone));
  }

  // ═══════════════════════════════════════════════════════════════════════
  // S18: model bramek zapisu (przelacznik hist_fs)
  // ═══════════════════════════════════════════════════════════════════════
  std::printf("\n[S18] Model bramek zapisu — przelacznik hist_fs\n");
  {
    struct Model {
      std::atomic<bool> wlaczony{true};
      std::vector<int> ram;          // pierscien 1440 probek (zawsze rosnie)
      std::string pending;           // historiaPendingCsv
      std::string plik;              // hist_short.csv
      int odZapisu = 0;
      int probek = 0;
      void probka(int t) {
        ++probek;
        ram.push_back(t);
        if (ram.size() > 1440) ram.erase(ram.begin());
        if (!wlaczony.load()) return;                 // bramka z historiaZapiszProbke
        pending += "linia(" + std::to_string(t) + ")\n";
        if (++odZapisu >= 5) { flush(); odZapisu = 0; }
      }
      bool flush() {                                   // bramka z historiaFlushDoFS (3.57.5)
        if (!wlaczony.load()) return false;
        if (pending.empty()) return true;
        plik += pending;
        pending.clear();
        return true;
      }
      int linii() const { return (int)std::count(plik.begin(), plik.end(), '\n'); }
      void przelacz(bool on) { wlaczony.store(on); if (on) flush(); }
    };

    {   // 1000 probek, zapis WLACZONY
      Model m;
      for (int i = 0; i < 1000; ++i) m.probka(i);
      check("WL: 1000 probek = 1000 linii w pliku", m.linii() == 1000, std::to_string(m.linii()));
      check("WL: brak resztki w kolejce", m.pending.empty());
      check("WL: RAM ma wszystkie 1000", (int)m.ram.size() == 1000);
    }
    {   // 1000 probek, zapis WYLACZONY
      Model m; m.przelacz(false);
      const size_t przed = m.plik.size();
      for (int i = 0; i < 1000; ++i) m.probka(i);
      check("WYL: plik nie urosl ani o bajt", m.plik.size() == przed && m.linii() == 0);
      check("WYL: kolejka pusta (nic nie zbieramy)", m.pending.empty());
      check("WYL: RAM mimo to ma 1000 probek", (int)m.ram.size() == 1000);
    }
    {   // wylaczenie z resztka w kolejce, potem ponowne wlaczenie
      Model m;
      for (int i = 0; i < 7; ++i) m.probka(i);        // 5 zapisane, 2 czekaja
      check("7 probek WL = 5 linii + 2 w kolejce", m.linii() == 5 && m.pending.size() > 0);
      m.przelacz(false);
      for (int i = 7; i < 107; ++i) m.probka(i);
      check("po wylaczeniu plik stoi na 5 liniach", m.linii() == 5, std::to_string(m.linii()));
      m.przelacz(true);   // 3.57.6: wlaczenie zglasza natychmiastowy flush
      check("po wlaczeniu resztka (2) dojezdza od razu", m.linii() == 7, std::to_string(m.linii()));
      for (int i = 107; i < 112; ++i) m.probka(i);
      // Licznik "co 5 probek" liczy sie dalej (2 z poprzedniego cyklu), wiec
      // flush wypada po 3 probkach: 3 nowe linie w pliku, 2 czekaja w kolejce.
      check("5 kolejnych probek: 3 w pliku, 2 w kolejce (nic nie zginelo)",
            m.linii() == 10 && std::count(m.pending.begin(), m.pending.end(), '\n') == 2,
            std::to_string(m.linii()));
    }
    {   // 50 przelaczen, kolejka czasu nigdy sie nie cofa i nic nie ginie
      Model m;
      int t = 0;
      bool kolejnosc = true;
      for (int cykl = 0; cykl < 50; ++cykl) {
        for (int i = 0; i < 3; ++i) m.probka(t++);
        m.przelacz(cykl % 2 == 0 ? false : true);
      }
      m.przelacz(true);
      // sprawdz, ze numery w pliku sa rosnace
      size_t a = 0;
      while (a < m.plik.size()) {
        size_t b = m.plik.find(')', a);
        if (b == std::string::npos) break;
        int v = std::atoi(m.plik.c_str() + a + 6);
        if (v < 0) kolejnosc = false;
        a = m.plik.find('\n', b);
        if (a == std::string::npos) break;
        ++a;
      }
      check("50 przelaczen: kolejnosc czasu rosnaca", kolejnosc);
      // Probki brane przy WYLACZONYM zapisie celowo nie trafiaja na flash:
      // 25 cykli z zapisem po 3 probki = 75 linii, reszta tylko w RAM.
      check("50 przelaczen: na flash tylko probki z wlaczonym zapisem (75)",
            m.linii() == 75, std::to_string(m.linii()));
      check("50 przelaczen: brak duplikatow linii", [&] {
        std::vector<std::string> l; size_t a = 0;
        while (a < m.plik.size()) { size_t b = m.plik.find('\n', a); if (b == std::string::npos) break;
          l.push_back(m.plik.substr(a, b - a)); a = b + 1; }
        std::vector<std::string> k = l; std::sort(k.begin(), k.end());
        return std::adjacent_find(k.begin(), k.end()) == k.end();
      }());
      check("50 przelaczen: RAM ma 150 probek", (int)m.ram.size() == 150);
    }
  }

  std::printf("\n%d OK / %d FAIL\n", ok, fail);
  return fail ? 1 : 0;
}
