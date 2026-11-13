// Atrapa Update.h (arduino-esp32, biblioteka Update) - zapis nowego programu.
// Od firmware'u v1.4.0 panel sam pobiera plik od centrali i oddaje go tutaj:
// Update.begin(rozmiar) -> Update.write(...) -> Update.end(false).
//
// Co atrapa modeluje (na podstawie Updater.cpp z arduino-esp32 3.3.8):
//   - begin(): rozmiar 0 albo wiekszy niz partycja -> blad UPDATE_ERROR_SIZE;
//     bierze ze sterty bufor jednego sektora (4096 B) i trzyma go do end() albo
//     abort() - tak samo jak biblioteka, wiec pomiar_pamieci.sh liczy go jako
//     pamiec firmware'u; brak pamieci na bufor (takze wymuszony przez
//     g_atrapa.otaBeginBezPamieci) -> false bez kodu bledu;
//   - write(): dane ida do g_atrapa.otaZapisane; kazde pelne 4096 B to jeden
//     sektor flash i g_atrapa.otaMsNaSektor czasu plytki (kasowanie + zapis
//     zatrzymuja program na plytce); blad zapisu po g_atrapa.otaBladZapisuPo bajtach;
//   - end(false): za malo bajtow -> blad UPDATE_ERROR_ABORT; potem to, co na
//     plytce robi ESP-IDF w esp_ota_set_boot_partition(): sprawdzenie obrazu.
//     Tutaj: pierwszy bajt 0xE9, uklad ESP32 (bajty 12-13 = 0), a gdy plik ma
//     dopisana sume (bajt 23 = 1) - SHA-256 tresci rowne ostatnim 32 bajtom.
//     Zly obraz -> UPDATE_ERROR_ACTIVATE. Dobry -> g_atrapa.otaUruchomiono++
//     (nowy program jest ustawiony jako startowy);
//   - abort(): porzuca zapis (g_atrapa.otaPorzucono++).
// CZEGO NIE MA: prawdziwej pamieci flash, segmentow programu i sumy kontrolnej
// naglowka, bootloadera ani powrotu do poprzedniego programu - to sprawdzi
// dopiero plytka.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <new>
#include "Arduino.h"
#include "atrapa_sha256.h"

#define UPDATE_ERROR_OK 0
#define UPDATE_ERROR_WRITE 1
#define UPDATE_ERROR_ERASE 2
#define UPDATE_ERROR_READ 3
#define UPDATE_ERROR_SPACE 4
#define UPDATE_ERROR_SIZE 5
#define UPDATE_ERROR_STREAM 6
#define UPDATE_ERROR_MD5 7
#define UPDATE_ERROR_MAGIC_BYTE 8
#define UPDATE_ERROR_ACTIVATE 9
#define UPDATE_ERROR_NO_PARTITION 10
#define UPDATE_ERROR_BAD_ARGUMENT 11
#define UPDATE_ERROR_ABORT 12
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#define U_FLASH 0

class UpdateClass {
 public:
  bool begin(size_t rozmiar = UPDATE_SIZE_UNKNOWN, int komenda = U_FLASH, int = -1, uint8_t = 0, const char* = nullptr) {
    (void)komenda;
    if (rozmiar_ > 0) return false;   // "already running"
    blad_ = UPDATE_ERROR_OK;
    g_atrapa.otaBeginWywolan++;
    if (rozmiar == 0) { blad_ = UPDATE_ERROR_SIZE; return false; }
    if (rozmiar == UPDATE_SIZE_UNKNOWN) rozmiar = g_atrapa.otaPartycjaB;
    else if (rozmiar > g_atrapa.otaPartycjaB) { blad_ = UPDATE_ERROR_SIZE; return false; }
    if (g_atrapa.otaBeginBezPamieci) return false;   // jak w bibliotece: false, kod bledu zostaje 0
    bufor_ = new (std::nothrow) uint8_t[4096];       // bufor sektora, jak _buffer w Updater.cpp
    if (bufor_ == nullptr) return false;
    {
      AtrapaBezPomiaru bezPomiaru;
      g_atrapa.otaZapisane.clear();
    }
    rozmiar_ = rozmiar;
    wSektorze_ = 0;
    return true;
  }
  size_t write(uint8_t* dane, size_t n) {
    if (hasError() || !isRunning()) return 0;
    if (n > remaining()) { porzuc(UPDATE_ERROR_SPACE); return 0; }
    if (g_atrapa.otaBladZapisuPo >= 0 && (long)(g_atrapa.otaZapisane.size() + n) > g_atrapa.otaBladZapisuPo) {
      porzuc(UPDATE_ERROR_WRITE);
      return 0;
    }
    {
      AtrapaBezPomiaru bezPomiaru;   // to "pamiec flash", nie RAM firmware'u
      g_atrapa.otaZapisane.insert(g_atrapa.otaZapisane.end(), dane, dane + n);
    }
    // Czas plytki: kazdy pelny sektor (4096 B) to kasowanie i zapis flash.
    wSektorze_ += n;
    while (wSektorze_ >= 4096) {
      wSektorze_ -= 4096;
      if (g_atrapa.otaMsNaSektor > 0) atrapa_uplyw_czasu(g_atrapa.otaMsNaSektor);
    }
    return n;
  }
  bool end(bool nawetNiepelny = false) {
    if (hasError() || rozmiar_ == 0) return false;
    if (!isFinished() && !nawetNiepelny) { porzuc(UPDATE_ERROR_ABORT); return false; }
    // To, co na plytce sprawdza ESP-IDF, zanim ustawi nowy program jako startowy.
    const std::vector<uint8_t>& p = g_atrapa.otaZapisane;
    bool dobry = p.size() >= 24 + 32 + 1 && p[0] == 0xE9 && p[12] == 0 && p[13] == 0;
    if (dobry && p[23] == 1) {
      uint8_t suma[32];
      atrapa_sha256(p.data(), p.size() - 32, suma);
      dobry = (memcmp(suma, p.data() + p.size() - 32, 32) == 0);
    }
    if (!dobry) { porzuc(UPDATE_ERROR_ACTIVATE); return false; }
    g_atrapa.otaUruchomiono++;
    rozmiar_ = 0;
    zwolnijBufor();
    return true;
  }
  void abort() { porzuc(UPDATE_ERROR_ABORT); }
  uint8_t getError() { return blad_; }
  void clearError() { blad_ = UPDATE_ERROR_OK; }
  bool hasError() { return blad_ != UPDATE_ERROR_OK; }
  bool isRunning() { return rozmiar_ > 0; }
  bool isFinished() { return g_atrapa.otaZapisane.size() == rozmiar_; }
  size_t size() { return rozmiar_; }
  size_t progress() { return g_atrapa.otaZapisane.size(); }
  size_t remaining() { return rozmiar_ - g_atrapa.otaZapisane.size(); }
  const char* errorString() {
    switch (blad_) {
      case UPDATE_ERROR_OK: return "No Error";
      case UPDATE_ERROR_WRITE: return "Flash Write Failed";
      case UPDATE_ERROR_SPACE: return "Not Enough Space";
      case UPDATE_ERROR_SIZE: return "Bad Size Given";
      case UPDATE_ERROR_ACTIVATE: return "Could Not Activate The Firmware";
      case UPDATE_ERROR_ABORT: return "Aborted";
      default: return "UNKNOWN";
    }
  }

 private:
  void porzuc(uint8_t blad) {
    if (rozmiar_ > 0) g_atrapa.otaPorzucono++;
    rozmiar_ = 0;
    blad_ = blad;
    zwolnijBufor();
  }
  void zwolnijBufor() {
    delete[] bufor_;
    bufor_ = nullptr;
  }
  size_t rozmiar_ = 0;
  size_t wSektorze_ = 0;
  uint8_t blad_ = UPDATE_ERROR_OK;
  uint8_t* bufor_ = nullptr;   // 4096 B od begin() do end()/abort()
};
inline UpdateClass Update;
