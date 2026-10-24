// ============================================================================
// [v3.55.2] Test hostowy: nadzor zadan sieciowych (tgTask / tgPollTask).
//
// Co sprawdza (to jest sedno poprawki, nie ozdoba):
//   1. Zdrowa centrala NIE jest restartowana — ani razu, w zadnym wariancie.
//   2. Zadanie, ktore milczy dluzej niz prog (5 min), daje decyzje RESTART
//      i wskazuje WLASCIWE zadanie (tgTask albo tgPollTask).
//   3. Prog dziala co do milisekundy: 299 999 ms = spokoj, 300 000 ms = restart.
//   4. Pierwsze 3 minuty po starcie nadzor spi (rozbieg) — inaczej urzadzenie
//      restartowaloby sie w kolko, zanim zadania zdaza wystartowac.
//   5. Podczas OTA i testow regresyjnych nadzor spi (tam dlugie milczenie jest
//      normalne i restart zepsulby aktualizacje).
//   6. Po 3 restartach z rzedu nadzor przestaje restartowac (LIMIT) — chroni
//      przed petla restartow przy trwalej awarii sprzetu.
//   7. Zawiniecie licznika millis (po ~49 dniach pracy) nie powoduje falszywego
//      alarmu ani nie ukrywa prawdziwego zawieszenia.
//
// Jak to dziala: run_nadzor_tests.py wycina z ../../main_centrala.cpp DOKLADNIE
// fragment z nadzorDecyzja() (czysta funkcja, bez Arduino i bez ESP32) i
// wstawia go w miejsce znacznika SLICE_NADZOR. Reszta to zwykly g++.
//
// Uzycie (z katalogu tests/host):  python3 run_nadzor_tests.py
// ============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// SLICE_NADZOR

// ============================================================================
// Mini-ramka testowa (bez zewnetrznych bibliotek — tak jak pozostale testy).
// ============================================================================
static int g_testow = 0;
static int g_bledow = 0;

static void sprawdz(bool warunek, const char *opis) {
  ++g_testow;
  if (warunek) {
    std::printf("  [OK]   %s\n", opis);
  } else {
    ++g_bledow;
    std::printf("  [BLAD] %s\n", opis);
  }
}

static NadzorWejscie wej(uint32_t now, uint32_t hbTg, uint32_t hbPoll,
                         uint32_t licznik, bool blokada) {
  NadzorWejscie w;
  w.nowMs = now;
  w.hbTg = hbTg;
  w.hbPoll = hbPoll;
  w.licznikRestartow = licznik;
  w.blokada = blokada;
  return w;
}

// Uruchamia decyzje i zwraca komplet wynikow w jednym miejscu.
static NadzorDecyzja policz(uint32_t now, uint32_t hbTg, uint32_t hbPoll,
                            uint32_t licznik, bool blokada,
                            uint8_t *kto, uint32_t *wiek) {
  NadzorWejscie w = wej(now, hbTg, hbPoll, licznik, blokada);
  return nadzorDecyzja(w, kto, wiek);
}

int main() {
  std::printf("=== Test nadzoru zadan (v3.55.2) ===\n\n");

  uint8_t kto = 0;
  uint32_t wiek = 0;
  NadzorDecyzja d;

  // --- 0. Stale: pilnujemy, zeby nikt ich po cichu nie zmienil -------------
  std::printf("Stale nadzoru:\n");
  sprawdz(NADZOR_PROG_MS == 300000UL, "prog milczenia = 300 000 ms (5 min)");
  sprawdz(NADZOR_ROZBIEG_MS == 180000UL, "rozbieg po starcie = 180 000 ms (3 min)");
  sprawdz(NADZOR_LICZNIK_WAZNY_MS == 1800000UL, "licznik wazny 30 min");
  sprawdz(NADZOR_MAX_RESTARTOW == 3U, "maksymalnie 3 restarty z rzedu");

  // --- 1. Zdrowa centrala: ZERO restartow ----------------------------------
  std::printf("\n1. Centrala zdrowa (obie beat'ty swieze):\n");
  d = policz(600000UL, 599000UL, 598000UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_BRAK, "decyzja = BRAK");
  sprawdz(kto == 0U, "nie wskazano zadnego zadania");
  sprawdz(wiek == 0U, "wiek = 0");

  // --- 2. Rozbieg po starcie ----------------------------------------------
  std::printf("\n2. Pierwsze 3 minuty po starcie:\n");
  d = policz(1000UL, 900UL, 900UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_USPIONY, "1 s po starcie = USPIONY");
  d = policz(179999UL, 100UL, 100UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_USPIONY, "179 999 ms = jeszcze USPIONY");
  // Dokladnie na granicy rozbiegu liczy sie juz normalna logika.
  d = policz(180000UL, 179000UL, 179000UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_BRAK, "180 000 ms (granica) = juz BRAK, nie USPIONY");

  // --- 3. tgTask milczy ----------------------------------------------------
  std::printf("\n3. tgTask milczy:\n");
  d = policz(400000UL, 399999UL, 399000UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_BRAK, "milczenie 1 ms ponizej progu = BRAK");
  d = policz(400000UL, 400000UL - NADZOR_PROG_MS, 399000UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_RESTART, "milczenie DOKLADNIE 300 000 ms = RESTART");
  sprawdz(kto == 1U, "wskazane tgTask");
  sprawdz(wiek == NADZOR_PROG_MS, "wiek = 300 000 ms");
  d = policz(900000UL, 100000UL, 899000UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_RESTART, "milczenie 800 s = RESTART");
  sprawdz(kto == 1U, "wskazane tgTask");
  sprawdz(wiek == 800000UL, "wiek = 800 000 ms");

  // --- 4. tgPollTask milczy ------------------------------------------------
  std::printf("\n4. tgPollTask milczy:\n");
  d = policz(900000UL, 899000UL, 100000UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_RESTART, "tgTask zyje, tgPollTask milczy = RESTART");
  sprawdz(kto == 2U, "wskazane tgPollTask (nie tgTask)");
  sprawdz(wiek == 800000UL, "wiek = 800 000 ms");

  // --- 5. OTA i testy regresyjne -------------------------------------------
  std::printf("\n5. OTA / testy regresyjne (blokada):\n");
  d = policz(900000UL, 100000UL, 100000UL, 0U, true, &kto, &wiek);
  sprawdz(d == NADZOR_USPIONY, "oba zadania milcza, ale trwa OTA = USPIONY");
  sprawdz(kto == 0U, "brak wskazania zadania");

  // --- 6. Limit restartow --------------------------------------------------
  std::printf("\n6. Limit restartow:\n");
  d = policz(900000UL, 100000UL, 899000UL, 2U, false, &kto, &wiek);
  sprawdz(d == NADZOR_RESTART, "licznik 2 = jeszcze wolno restartowac");
  d = policz(900000UL, 100000UL, 899000UL, 3U, false, &kto, &wiek);
  sprawdz(d == NADZOR_LIMIT, "licznik 3 = LIMIT, koniec restartow");
  sprawdz(kto == 1U, "przy limicie nadal wiadomo, co milczy");
  d = policz(900000UL, 100000UL, 899000UL, 99U, false, &kto, &wiek);
  sprawdz(d == NADZOR_LIMIT, "licznik 99 = nadal LIMIT (bez petli)");

  // --- 7. Zadania jeszcze nie wystartowaly --------------------------------
  std::printf("\n7. Zadania jeszcze nie ruszyly:\n");
  d = policz(600000UL, 0U, 0U, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_USPIONY, "oba beat'ty 0 = USPIONY (nie restartujemy)");
  d = policz(600000UL, 599000UL, 0U, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_BRAK, "tgTask zyje, tgPollTask jeszcze nie rusza = BRAK");
  d = policz(600000UL, 0U, 100000UL, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_RESTART, "tgTask nigdy nie ruszyl, tgPollTask milczy = RESTART");
  sprawdz(kto == 2U, "wskazany tgPollTask");

  // --- 8. Zawiniecie millis (~49,7 dnia pracy) -----------------------------
  std::printf("\n8. Zawiniecie licznika millis:\n");
  // hb ustawiony "przed" zawinieciem, now juz "po".
  const uint32_t hbPrzedZawinieciem = 0xFFFF0000UL;   // 4294901760
  d = policz(200000UL, hbPrzedZawinieciem, hbPrzedZawinieciem, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_BRAK, "wiek 265 536 ms po zawinieciu = BRAK (bez falszywego alarmu)");
  d = policz(400000UL, hbPrzedZawinieciem, hbPrzedZawinieciem, 0U, false, &kto, &wiek);
  sprawdz(d == NADZOR_RESTART, "wiek 465 536 ms po zawinieciu = RESTART");
  sprawdz(wiek == 465536UL, "wiek policzony poprawnie po zawinieciu");

  // --- 9. Wskaznik "kto" moze byc pusty (API odporne na nullptr) ----------
  std::printf("\n9. Wywolanie bez wskaznikow wyjsciowych:\n");
  NadzorWejscie w = wej(900000UL, 100000UL, 899000UL, 0U, false);
  d = nadzorDecyzja(w, nullptr, nullptr);
  sprawdz(d == NADZOR_RESTART, "decyzja liczona bez wskaznikow = RESTART");

  // ------------------------------------------------------------------------
  std::printf("\n=== Wynik: %d sprawdzen, %d bledow ===\n", g_testow, g_bledow);
  if (g_bledow == 0) {
    std::printf("WYNIK KONCOWY: OK\n");
    return 0;
  }
  std::printf("WYNIK KONCOWY: BLAD\n");
  return 1;
}
