#pragma once
// 211: pamiec plikow jako history193::Calls - ten sam interfejs VFS, ktorego
// uzywa prawdziwy history193::Engine i nowa zapinka. Na urzadzeniu pod tym
// interfejsem jest esp_littlefs (EspVfsCalls.h); tu jest mapa nazwa->bajty,
// z wstrzykiwaniem awarii (krotki odczyt, blad odczytu, blad zamkniecia).
//
// Model NIE udaje LittleFS: nie ma sektorow, inode'ow, opoznien ani zaniku
// zasilania. Sprawdza wylacznie umowe interfejsu i to, co kod z niego robi.
#include <cerrno>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "CheckedHistoryIO.h"

class RamCalls final : public history193::Calls {
 public:
  // --- instrumentacja dla testow ---
  int otwartych = 0;             // ile uchwytow jest teraz otwartych
  int otwarciem = 0;             // ile razy ktos otworzyl plik (lacznie)
  int zamkniec = 0;              // ile razy ktos zamknal plik (lacznie)
  long long odczytane = 0;       // bajtow wydanych przez read()
  long long zapisane = 0;        // bajtow przyjetych przez write()

  // --- wstrzykiwane awarie ---
  int awariaOdczytuPo = -1;   // >=0: po tylu bajtach read() zwraca -1/EIO
  bool awariaZamkniecia = false;
  bool odmowaOtwarciu = false;

  void wyczysc() {
    pliki_.clear();
    otwarte_.clear();
    otwartych = 0;
    otwarciem = 0;
    zamkniec = 0;
    odczytane = 0;
    zapisane = 0;
    awariaOdczytuPo = -1;
    awariaZamkniecia = false;
    odmowaOtwarciu = false;
    przyOtwarciu = nullptr;  // hook testu nie moze przezyc swojego zakresu
    nastepnyFd_ = 3;
    err_ = 0;
  }

  void utworz(const std::string &p, const std::string &tresc) { pliki_[p].dane.assign(tresc.begin(), tresc.end()); }
  void dopisz(const std::string &p, const std::string &tresc) {
    auto &d = pliki_[p].dane;
    d.insert(d.end(), tresc.begin(), tresc.end());
  }
  void przytnij(const std::string &p, size_t n) {
    auto &d = pliki_[p].dane;
    if (d.size() > n) d.resize(n);
  }
  std::string tresc(const std::string &p) {
    auto it = pliki_.find(p);
    return it == pliki_.end() ? std::string() : std::string(it->second.dane.begin(), it->second.dane.end());
  }
  size_t rozmiar(const std::string &p) {
    auto it = pliki_.find(p);
    return it == pliki_.end() ? 0 : it->second.dane.size();
  }
  bool istnieje(const std::string &p) { return pliki_.find(p) != pliki_.end(); }

  // Wywolywane na poczatku open() - testy sprawdzaja tym, czy zapinka
  // otwiera plik pod blokada pisarza.
  std::function<void(const char *)> przyOtwarciu;

  // --- history193::Calls ---
  int open(const char *p, int flags) override {
    err_ = 0;
    if (przyOtwarciu) przyOtwarciu(p);
    if (odmowaOtwarciu) {
      err_ = EIO;
      return -1;
    }
    auto it = pliki_.find(std::string(p));
    if (it == pliki_.end()) {
      if ((flags & O_CREAT) == 0) {
        err_ = ENOENT;
        return -1;
      }
      pliki_[std::string(p)] = Plik();
    }
    const int fd = nastepnyFd_++;
    Uchwyt u;
    u.path = p;
    u.pos = 0;
    u.flags = flags;
    otwarte_[fd] = u;
    ++otwartych;
    ++otwarciem;
    return fd;
  }

  int stat(int fd, struct stat *s) override {
    err_ = 0;
    Uchwyt *u = znajdz(fd);
    if (!u) return -1;
    std::memset(s, 0, sizeof(*s));
    s->st_mode = S_IFREG | 0600;
    s->st_size = static_cast<off_t>(pliki_[u->path].dane.size());
    return 0;
  }

  off_t seek(int fd, off_t p) override {
    err_ = 0;
    Uchwyt *u = znajdz(fd);
    if (!u || p < 0) {
      err_ = u ? EINVAL : EBADF;
      return -1;
    }
    u->pos = static_cast<size_t>(p);
    return p;
  }

  ssize_t read(int fd, void *b, std::size_t n) override {
    err_ = 0;
    Uchwyt *u = znajdz(fd);
    if (!u) return -1;
    if (awariaOdczytuPo >= 0 && odczytane >= awariaOdczytuPo) {
      err_ = EIO;
      return -1;
    }
    std::vector<char> &d = pliki_[u->path].dane;
    if (u->pos >= d.size()) return 0;  // EOF
    size_t k = d.size() - u->pos;
    if (k > n) k = n;
    std::memcpy(b, d.data() + u->pos, k);
    u->pos += k;
    odczytane += static_cast<long long>(k);
    return static_cast<ssize_t>(k);
  }

  ssize_t write(int fd, const void *b, std::size_t n) override {
    err_ = 0;
    Uchwyt *u = znajdz(fd);
    if (!u) return -1;
    std::vector<char> &d = pliki_[u->path].dane;
    if (u->pos > d.size()) d.resize(u->pos, 0);
    for (size_t i = 0; i < n; ++i) {
      const char c = static_cast<const char *>(b)[i];
      if (u->pos + i < d.size()) d[u->pos + i] = c;
      else d.push_back(c);
    }
    u->pos += n;
    zapisane += static_cast<long long>(n);
    return static_cast<ssize_t>(n);
  }

  int sync(int fd) override {
    err_ = 0;
    return znajdz(fd) ? 0 : -1;
  }

  int close(int fd) override {
    err_ = 0;
    auto it = otwarte_.find(fd);
    if (it == otwarte_.end()) {
      err_ = EBADF;
      return -1;
    }
    otwarte_.erase(it);
    --otwartych;
    ++zamkniec;
    if (awariaZamkniecia) {
      err_ = EIO;
      return -1;
    }
    return 0;
  }

  int rename(const char *a, const char *b) override {
    err_ = 0;
    auto it = pliki_.find(std::string(a));
    if (it == pliki_.end()) {
      err_ = ENOENT;
      return -1;
    }
    pliki_[std::string(b)] = it->second;
    pliki_.erase(it);
    return 0;
  }

  int error() const override { return err_; }

 private:
  struct Plik {
    std::vector<char> dane;
  };
  struct Uchwyt {
    std::string path;
    size_t pos = 0;
    int flags = 0;
  };
  Uchwyt *znajdz(int fd) {
    auto it = otwarte_.find(fd);
    if (it == otwarte_.end()) {
      err_ = EBADF;
      return nullptr;
    }
    return &it->second;
  }

  std::map<std::string, Plik> pliki_;
  std::map<int, Uchwyt> otwarte_;
  int nastepnyFd_ = 3;
  int err_ = 0;
};
