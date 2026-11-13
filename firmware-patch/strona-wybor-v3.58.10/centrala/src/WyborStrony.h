#pragma once
#include <atomic>
#include <stdint.h>
// Wybor wspolny dla calej centrali. Operacje zapisu musza byc serializowane
// przez wywolujacego; odczyt na watku HTTP nie wymaga blokady.
class WyborStrony {
 public:
  void przywroc(uint8_t v) { value_.store(v == 1 ? 1 : 0); }
  uint8_t odczytaj() const { return value_.load(); }
  template <class Save> bool ustaw(uint8_t v, Save save) {
    if (v > 1) return false;
    if (v == value_.load()) return true; // oszczedzanie NVS
    if (!save(v)) return false;
    value_.store(v); return true;
  }
 private:
  std::atomic<uint8_t> value_{0};  // 0=nowa (domyslna), 1=stara
};
