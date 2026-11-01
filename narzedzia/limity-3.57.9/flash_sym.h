#pragma once
// Symulowany "dysk" LittleFS o SKONCZONEJ pojemnosci.
//
// Podstawiamy go pod history193::Calls - czyli pod DOKLADNIE te warstwe,
// z ktorej korzysta firmware (src/CheckedHistoryIO.h, kopia bit w bit z
// paczki 3.57.8, SHA-256 8eb18693...). Dzieki temu symulacja nie jest
// przepisem "jak to dziala", tylko uruchomieniem prawdziwego Engine::append()
// i Engine::trim() na dysku, ktory potrafi powiedziec "nie ma miejsca".
//
// Model:
//  * pojemnosc liczona jak w LittleFS - zajete bloki po `block` bajtow,
//    a nie surowe bajty pliku (male pliki tez zajmuja caly blok),
//  * zapis ponad pojemnosc konczy sie ENOSPC i NIC nie jest dopisywane
//    (najostrozniejszy wariant: realny littlefs moze zapisac czesc),
//  * rename() podmienia cel (miejsce po starym celu wraca do puli),
//  * O_EXCL na istniejacym pliku konczy sie EEXIST - tak jak w POSIX.
#include "CheckedHistoryIO.h"

#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace limity {

class FakeFlash final : public history193::Calls {
 public:
  struct Plik {
    std::vector<char> dane;
  };

  FakeFlash(std::size_t pojemnoscBajtow, std::size_t blokBajtow = 4096)
      : pojemnosc_(pojemnoscBajtow), blok_(blokBajtow ? blokBajtow : 1) {}

  // ── historia193::Calls ───────────────────────────────────────────────
  int open(const char* sciezka, int flagi) override {
    err_ = 0;
    const std::string p(sciezka);
    auto it = pliki_.find(p);
    const bool istnieje = (it != pliki_.end());
    if (istnieje && (flagi & O_CREAT) && (flagi & O_EXCL)) {
      err_ = EEXIST;
      return -1;
    }
    if (!istnieje && !(flagi & O_CREAT)) {
      err_ = ENOENT;
      return -1;
    }
    if (!istnieje) pliki_[p] = Plik{};
    const int fd = nastepnyFd_++;
    fds_[fd] = Fd{p, 0};
    return fd;
  }

  int stat(int fd, struct stat* st) override {
    err_ = 0;
    auto f = fds_.find(fd);
    if (f == fds_.end()) {
      err_ = EBADF;
      return -1;
    }
    std::memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG | 0644;
    st->st_size = static_cast<off_t>(pliki_[f->second.sciezka].dane.size());
    return 0;
  }

  off_t seek(int fd, off_t pozycja) override {
    err_ = 0;
    auto f = fds_.find(fd);
    if (f == fds_.end()) {
      err_ = EBADF;
      return -1;
    }
    f->second.pozycja = pozycja;
    return pozycja;
  }

  ssize_t read(int fd, void* bufor, std::size_t n) override {
    err_ = 0;
    auto f = fds_.find(fd);
    if (f == fds_.end()) {
      err_ = EBADF;
      return -1;
    }
    const auto& dane = pliki_[f->second.sciezka].dane;
    const std::size_t koniec = dane.size();
    const std::size_t od = static_cast<std::size_t>(f->second.pozycja);
    if (od >= koniec) return 0;
    const std::size_t ile = (koniec - od < n) ? (koniec - od) : n;
    std::memcpy(bufor, dane.data() + od, ile);
    f->second.pozycja += static_cast<off_t>(ile);
    ++odczyty_;
    return static_cast<ssize_t>(ile);
  }

  ssize_t write(int fd, const void* bufor, std::size_t n) override {
    err_ = 0;
    auto f = fds_.find(fd);
    if (f == fds_.end()) {
      err_ = EBADF;
      return -1;
    }
    auto& plik = pliki_[f->second.sciezka];
    const std::size_t stare = plik.dane.size();
    const std::size_t od = static_cast<std::size_t>(f->second.pozycja);
    const std::size_t nowe = (od + n > stare) ? (od + n) : stare;
    const long long koszt = static_cast<long long>(zajeteBloki(nowe)) -
                            static_cast<long long>(zajeteBloki(stare));
    if (uzyte_ + koszt > static_cast<long long>(pojemnosc_)) {
      ++odrzuconeZapisy;
      err_ = ENOSPC;
      return -1;
    }
    if (nowe > stare) plik.dane.resize(nowe, 0);
    std::memcpy(plik.dane.data() + od, bufor, n);
    f->second.pozycja += static_cast<off_t>(n);
    uzyte_ += koszt;
    if (uzyte_ > szczyt) szczyt = uzyte_;
    ++zapisy_;
    return static_cast<ssize_t>(n);
  }

  int sync(int fd) override {
    err_ = 0;
    return (fds_.count(fd) == 0) ? (err_ = EBADF, -1) : 0;
  }

  int close(int fd) override {
    err_ = 0;
    auto f = fds_.find(fd);
    if (f == fds_.end()) {
      err_ = EBADF;
      return -1;
    }
    fds_.erase(f);
    return 0;
  }

  int rename(const char* z, const char* na) override {
    err_ = 0;
    auto it = pliki_.find(z);
    if (it == pliki_.end()) {
      err_ = ENOENT;
      return -1;
    }
    const std::string cel(na);
    // Podmiana celu: miejsce po starym celu wraca do puli.
    auto stary = pliki_.find(cel);
    if (stary != pliki_.end()) {
      uzyte_ -= static_cast<long long>(zajeteBloki(stary->second.dane.size()));
      pliki_.erase(stary);
    }
    pliki_[cel] = std::move(it->second);
    pliki_.erase(it);
    ++zmianyNazw_;
    return 0;
  }

  int error() const override { return err_; }

  // ── narzedzia symulacji ─────────────────────────────────────────────
  std::size_t rozmiar(const std::string& p) const {
    auto it = pliki_.find(p);
    return (it == pliki_.end()) ? 0 : it->second.dane.size();
  }

  std::size_t linii(const std::string& p) const {
    auto it = pliki_.find(p);
    if (it == pliki_.end()) return 0;
    std::size_t n = 0;
    for (char c : it->second.dane)
      if (c == '\n') ++n;
    return n;
  }

  bool istnieje(const std::string& p) const { return pliki_.count(p) != 0; }

  // Obcina ostatnie `bajty` znakow z pliku (model uszkodzonego ogona).
  void obetnijOgon(const std::string& p, std::size_t bajty) {
    auto it = pliki_.find(p);
    if (it == pliki_.end()) return;
    auto& dane = it->second.dane;
    const std::size_t przed = dane.size();
    const std::size_t po = (bajty < przed) ? (przed - bajty) : 0;
    dane.resize(po);
    uzyte_ -= static_cast<long long>(zajeteBloki(przed)) -
              static_cast<long long>(zajeteBloki(po));
  }

  // Zajete miejsce liczone po blokach (tak jak realny LittleFS).
  long long uzyte() const { return uzyte_; }
  std::size_t pojemnosc() const { return pojemnosc_; }
  std::size_t blok() const { return blok_; }

  // Dolacza plik "obcy" (np. log_a.txt/log_b.txt), ktory pozera miejsce.
  bool wypelnij(const std::string& p, std::size_t bajtow) {
    const long long koszt = static_cast<long long>(zajeteBloki(bajtow));
    if (uzyte_ + koszt > static_cast<long long>(pojemnosc_)) return false;
    pliki_[p].dane.assign(bajtow, 'L');
    uzyte_ += koszt;
    return true;
  }

  long long zapisy_ = 0;
  long long odczyty_ = 0;
  long long zmianyNazw_ = 0;
  long long odrzuconeZapisy = 0;
  long long szczyt = 0;  // najwyzsze zajecie dysku, jakie widzial model

 private:
  std::size_t zajeteBloki(std::size_t n) const {
    return ((n + blok_ - 1) / blok_) * blok_;
  }

  struct Fd {
    std::string sciezka;
    off_t pozycja = 0;
  };

  std::size_t pojemnosc_;
  std::size_t blok_ = 4096;
  std::map<std::string, Plik> pliki_;
  std::map<int, Fd> fds_;
  int nastepnyFd_ = 1;
  int err_ = 0;
  long long uzyte_ = 0;
};

}  // namespace limity
