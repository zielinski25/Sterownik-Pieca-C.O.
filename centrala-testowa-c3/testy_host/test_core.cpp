// test_core.cpp
// ============================================================================
// TEST RDZENIA NA PC (bez plytki, bez PlatformIO)
// ============================================================================
// Kompiluje przenosna czesc projektu (symulacja.cpp + naglowki) zwyklym g++,
// rozkreca symulacje o kilka godzin "wirtualnego" czasu i wypisuje na stdout:
//
//   ===STATUS===        JSON /api/status
//   ===USTAWIENIA===    JSON /api/ustawienia-get
//   ===WALIDACJA===     wyniki ustaw() dla przypadkow poprawnych/blędnych
//   ===CSV_KROTKA===    14-kolumnowy CSV (/api/history)
//   ===CSV_DLUGA===     35-kolumnowy CSV (/api/history/long)
//
// Wynik jest nastepnie sprawdzany przez sprawdz_wynik.py (poprawnosc JSON,
// komplet kluczy, ktore czyta panel LCD, liczba kolumn CSV).
//
// Uzycie:
//   g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/test_core test_core.cpp ../src/symulacja.cpp
//   /tmp/test_core | python3 sprawdz_wynik.py
// (albo po prostu ./uruchom_testy.sh)
// ============================================================================
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../src/symulacja.h"

static void pisarzStdout(void* ctx, const char* dane, size_t dlugosc) {
  (void)ctx;
  fwrite(dane, 1, dlugosc, stdout);
}

static wsp::Czas czasZUnix(uint32_t unixSek, bool ok = true) {
  wsp::Czas c;
  c.unix = unixSek;
  c.ok = ok;
  if (!ok) return c;
  time_t t = (time_t)unixSek;
  struct tm tm;
  gmtime_r(&t, &tm);
  c.rok = tm.tm_year + 1900;
  c.miesiac = tm.tm_mon + 1;
  c.dzien = tm.tm_mday;
  c.godzina = tm.tm_hour;
  c.minuta = tm.tm_min;
  c.sekunda = tm.tm_sec;
  return c;
}

int main() {
  sym::reset();
  ustaw::Stan u;

  // Start: 5 listopada 2026, 08:00 UTC — sezon grzewczy, ranek (slonce wschodzi).
  uint32_t unixSek = 1793952000u;  // 2026-11-05 08:00:00 UTC
  unsigned long terazMs = 0;

  // 3 godziny symulacji, krok 1 s — daje 180 probek historii krotkiej
  // i 3 domkniete godziny historii dlugiej.
  for (int i = 0; i < 3 * 3600; i++) {
    terazMs += 1000;
    unixSek += 1;
    sym::tick(terazMs, czasZUnix(unixSek), u);
  }

  sym::Kontekst k;
  k.ip = "192.168.4.1";
  k.rssi = 0;
  k.internet = false;
  k.wifiMode = "AP";
  k.wifiSsid = "";
  k.panelFwZgloszony = "v1.1.0";
  k.panelResetPowod = "deepsleep";
  k.terazMs = terazMs;
  k.uptimeS = terazMs / 1000UL;
  k.heapFree = 190000;
  k.stacje = 1;

  static char bufor[4096];
  printf("===STATUS===\n");
  size_t dl = sym::zbudujStatus(bufor, sizeof(bufor), k, u, czasZUnix(unixSek));
  printf("%s\n", bufor);
  printf("===STATUS_DLUGOSC=== %u\n", (unsigned)dl);

  printf("===USTAWIENIA===\n");
  dl = sym::zbudujUstawienia(bufor, sizeof(bufor), u);
  printf("%s\n", bufor);
  printf("===USTAWIENIA_DLUGOSC=== %u\n", (unsigned)dl);

  // --- Walidacja ustawien: przypadki poprawne i bledne (komunikaty 1:1 z centrali) ---
  printf("===WALIDACJA===\n");
  {
    struct Przypadek {
      const char* pole;
      long wartosc;
      bool oczekiwaneOk;
    } przypadki[] = {
        {"czasOn", 15, true},
        {"czasOn", 0, false},            // poza zakresem 1-180
        {"czasOn", 181, false},
        {"tempOn", 40, false},           // musi byc >= tempOff+2 (tempOff=50)
        {"tempOff", 60, false},          // musi byc <= tempOn-2
        {"progAlarmDym", 20, false},     // <= HISTEREZA_DYMU
        {"dymCzasStabilizacji", 55, false},  // >= dymCzasOn-5
        {"dymCzasOn", 25, false},        // <= dymCzasStabilizacji+5
        {"wybor", 4, false},
        {"trybSerwa", 2, true},
        {"nieznanePole", 1, false},
        {"mieszadloCzasOff", 60, true},
    };
    ustaw::Stan kopia = u;
    for (const auto& p : przypadki) {
      char err[160] = {0};
      bool ok = ustaw::ustaw(kopia, p.pole, p.wartosc, err, sizeof(err));
      printf("%s|%ld|%s|%s\n", p.pole, p.wartosc, ok ? "OK" : "ERR",
             ok ? "" : err);
      if (ok != p.oczekiwaneOk) {
        printf("!! NIEZGODNOSC OCZEKIWANIA: %s=%ld -> %s\n", p.pole, p.wartosc,
               ok ? "OK" : "ERR");
      }
    }
    // Klucze NVS musza miec <= 15 znakow (patrz [FIX NVS-15]).
    const char* pola[] = {"wybor", "trybSerwa", "czasOn", "czasOff", "tempOn",
                          "tempOff", "antystopWlaczony", "antystopDni",
                          "mieszadloWlaczony", "mieszadloCzasOn",
                          "mieszadloCzasOff", "tempZadServo", "histerServo",
                          "skokKlapy", "skokSyberka", "odchylTemp", "mnoznik",
                          "progAlarmTemp", "progAlarmDym", "dymCzasOn",
                          "dymCzasOff", "dymCzasStabilizacji", "dymProgTemp",
                          "dymTrybPracy"};
    for (const char* pole : pola) {
      const char* klucz = ustaw::kluczNvs(pole);
      size_t n = klucz ? strlen(klucz) : 0;
      printf("NVS|%s|%s|%u\n", pole, klucz ? klucz : "(brak)", (unsigned)n);
      if (klucz && n > 15) printf("!! KLUCZ NVS ZA DLUGI: %s\n", klucz);
    }
  }

  // --- CSV ---
  printf("===CSV_KROTKA===\n");
  sym::historiaCsv(false, pisarzStdout, nullptr);
  printf("===CSV_DLUGA===\n");
  sym::historiaCsv(true, pisarzStdout, nullptr);
  printf("===KONIEC===\n");
  return 0;
}
