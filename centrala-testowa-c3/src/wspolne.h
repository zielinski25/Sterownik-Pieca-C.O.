// wspolne.h
// ============================================================================
// CENTRALA TESTOWA NA ESP32-C3 — czesc WSPOLNA rdzenia
// ============================================================================
// Ten naglowek NIE zawiera NIC specyficznego dla Arduino/ESP — dzieki temu
// cala logika (ustawienia, symulacja, budowanie JSON, CSV historii) kompiluje
// sie i daje sie uruchomic TAKZE na PC (patrz testy_host/), bez plytki.
//
// Konwencja: brak String/Arduino — wszystko na char[] + snprintf + wlasny
// formater liczb zmiennoprzecinkowych. Powod: String na ESP32 fragmentuje
// stert i psuje sie inaczej niz std::string na PC; poza tym formater wlasny
// daje IDENTYCZNY wynik na obu platformach (bez roznic newlib-nano vs glibc).
// ============================================================================
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace wsp {

// ----------------------------------------------------------------------------
// Czas cywilny — policzony w warstwie Arduino (localtime_r) i podany rdzeniowi.
// `ok == false` znaczy "brak wiarygodnego czasu" i jest mapowane 1:1 na
// "rtc_ok":false w /api/status (panel pokazuje wtedy "--:--" i nie przelacza
// sezonu po miesiacu — dokladnie jak przy centrali bez RTC/NTP).
// ----------------------------------------------------------------------------
struct Czas {
  int rok = 0, miesiac = 0, dzien = 0;
  int godzina = 0, minuta = 0, sekunda = 0;
  // Czas unix (sekundy, UTC) — potrzebny wylacznie do kolumny czas_unix
  // w CSV historii (kolumna 13 krotkiego / 34 dlugiego bufora).
  uint32_t unix = 0;
  bool ok = false;
};

// Sygnatura "pisarza" — rdzen nigdy nie buduje calego CSV w pamieci, tylko
// oddaje kolejne wiersze warstwie Arduino, ktora wysyla je od razu do gniazda
// (patrz historiaCsv()). Na ESP32-C3 (bez PSRAM) to roznica ~25 KB szczytu.
typedef void (*PisarzFn)(void* ctx, const char* dane, size_t dlugosc);

// ----------------------------------------------------------------------------
// Formatowanie liczby zmiennoprzecinkowej bez zaleznosci od implementacji
// printf() w danym toolchainie. Zachowuje sie jak String(v, miejsca) z
// centrali: zaokraglenie połówkowe w gore od zera, zawsze `miejsca` cyfr.
// ----------------------------------------------------------------------------
inline size_t formatFloat(char* out, size_t outLen, float v, int miejsca) {
  if (outLen == 0) return 0;
  if (isnan(v) || isinf(v)) {
    snprintf(out, outLen, "--");
    return strlen(out);
  }
  const bool ujemna = (v < 0.0f);
  double bezwzgledna = ujemna ? -(double)v : (double)v;
  // przesuniecie o `miejsca` cyfr + zaokraglenie (połówki od zera, jak
  // String(v, miejsca) w centrali — nie "bankers rounding" z printf)
  double skala = 1.0;
  for (int i = 0; i < miejsca; i++) skala *= 10.0;
  bezwzgledna = floor(bezwzgledna * skala + 0.5) / skala;
  // %.*f z double — bezpieczne na obu platformach (problem newlib-nano dotyczy
  // tylko float przekazanego wprost do varargów)
  snprintf(out, outLen, "%s%.*f", ujemna ? "-" : "", miejsca, bezwzgledna);
  return strlen(out);
}

// Zaokraglenie do najblizszej liczby calkowitej (połówki od zera) — odpowiednik
// round() z ograniczeniem do zakresu int.
inline int roundInt(float v) {
  if (isnan(v)) return 0;
  double zaokr = (v < 0) ? -floor(-(double)v + 0.5) : floor((double)v + 0.5);
  if (zaokr > 2147483000.0) return 2147483000;
  if (zaokr < -2147483000.0) return -2147483000;
  return (int)zaokr;
}

template <typename T>
inline T ogranicz(T v, T dol, T gora) {
  return (v < dol) ? dol : ((v > gora) ? gora : v);
}

// ----------------------------------------------------------------------------
// Prosty, bezpieczny budowniczy JSON-a na buforze o stalym rozmiarze.
// `ok == false` = zabraklo miejsca (nigdy nie piszemy poza bufor, ale tez
// nigdy nie udajemy, ze JSON jest poprawny — patrz testy_host, ktore tego
// pilnuja).
// ----------------------------------------------------------------------------
class JsonBuf {
 public:
  JsonBuf(char* dane, size_t rozmiar) : d_(dane), n_(rozmiar) {
    if (n_ > 0) d_[0] = '\0';
  }

  bool ok() const { return ok_; }
  size_t dlugosc() const { return i_; }

  void raw(const char* s) {
    while (*s) znak(*s++);
  }

  void pole(const char* klucz, const char* wartosc, bool cudzyslow = true) {
    przecinek();
    cudzyslowZnak();
    raw(klucz);
    cudzyslowZnak();
    znak(':');
    if (cudzyslow) cudzyslowZnak();
    raw(wartosc ? wartosc : "");
    if (cudzyslow) cudzyslowZnak();
  }

  void poleInt(const char* klucz, long v) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%ld", v);
    pole(klucz, tmp, false);
  }

  void poleFloat(const char* klucz, float v, int miejsca) {
    char tmp[24];
    formatFloat(tmp, sizeof(tmp), v, miejsca);
    pole(klucz, tmp, false);
  }

  void poleBool(const char* klucz, bool v) {
    pole(klucz, v ? "true" : "false", false);
  }

  void poczatek() {
    znak('{');
    pierwsze_ = true;
  }

  void koniec() {
    znak('}');
  }

 private:
  void przecinek() {
    if (!pierwsze_) znak(',');
    pierwsze_ = false;
  }
  void cudzyslowZnak() { znak('"'); }
  void znak(char c) {
    if (!ok_) return;
    if (i_ + 1 >= n_) {  // +1 na terminator
      ok_ = false;
      return;
    }
    d_[i_++] = c;
    d_[i_] = '\0';
  }

  char* d_;
  size_t n_;
  size_t i_ = 0;
  bool ok_ = true;
  bool pierwsze_ = true;
};

}  // namespace wsp
