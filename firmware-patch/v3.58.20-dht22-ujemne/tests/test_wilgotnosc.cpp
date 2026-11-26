// Test na PC PRAWDZIWEGO zadania wilgotnosc() z main_centrala.cpp v3.58.20
// ([1]-[9] = testy z paczki 3.58.10 bez zmian - regresja; [10]-[11] = [v3.58.20] klon U2).
// (wycinek: wytnij_wilgotnosc.py -> wilgotnosc_wycinek.h) z atrapami sprzetu.
// Sprawdza to, czego test_zew2_filtr.cpp nie widzi: wspolprace filtra z bramka
// 120 s / 2 s w zadaniu (powtorka po ~3 s, [v3.58.10] ponowienie po bledzie po ~6 s),
// zapis do zapiszTempDiag(), wygaszanie po 255 s, wilgotnosc przy zlej ramce, symulacje, logi.
// Zadanie w centrali chodzi co 3000 ms (dodajZadanie(wilgotnosc, 3000)) - tu tak samo.
// Kompilacja: g++ -std=c++17 -Wall -Wextra -Werror -I<katalog z wycinkiem> test_wilgotnosc.cpp
#include <math.h>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ── atrapy ESP32 / Arduino / centrali ──────────────────────────────────────
static uint32_t g_ms = 0;                 // zegar testu (millis())
static uint32_t millis() { return g_ms; }
static int64_t esp_timer_get_time() { return (int64_t)g_ms * 1000LL; }
struct portMUX_TYPE { int x; };
#define portMUX_INITIALIZER_UNLOCKED {0}
static void portENTER_CRITICAL(portMUX_TYPE*) {}
static void portEXIT_CRITICAL(portMUX_TYPE*) {}
struct String {                           // tylko to, czego uzywa jsonTempDiag()
  std::string s;
  String() {}
  String(const char* c) : s(c) {}
  String(float v, int dec) { char b[32]; std::snprintf(b, sizeof b, "%.*f", dec, (double)v); s = b; }
  const char* c_str() const { return s.c_str(); }
};
enum DiagCategory : uint8_t { DC_SYSTEM = 0, DC_BOOT, DC_SENSOR };
enum DiagLevel : uint8_t { DL_ERR = 0, DL_WARN, DL_INFO, DL_DEBUG, DL_TRACE };
static std::vector<std::string> g_logi;
static void diagEmitf(DiagCategory, DiagLevel, const char* fmt, ...) {
  char b[512]; va_list ap; va_start(ap, fmt); std::vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  g_logi.push_back(b);
}
#define DLOGF(cat, level, fmt, ...) diagEmitf((cat), (level), (fmt), ##__VA_ARGS__)
static const uint8_t DHT_OK = 0, DHT_TIMEOUT = 2, DHT_CHECKSUM = 6;
static const uint8_t DHT22 = 22;
#define DHT_PIN 6
enum TelemetryChannel : uint8_t { TELEMETRY_CH_RH = 8 };
static int g_telAttempt = 0, g_telInvalid = 0, g_telAccepted = 0, g_telAcceptedSim = 0;
static uint64_t telemetryMonotonicMs() { return g_ms; }
static void telemetryMarkAttempt(TelemetryChannel, uint64_t) { ++g_telAttempt; }
static void telemetryMarkInvalid(TelemetryChannel) { ++g_telInvalid; }
static void telemetryMarkAccepted(TelemetryChannel, uint64_t, bool sim) { if (sim) ++g_telAcceptedSim; else ++g_telAccepted; }
static uint32_t czasOstatniegoPomiaru = 0, interwalPomiaru = 600000;
static void ustawInterwalPomiaru() {}
static bool symulacjaWilgotnosciAktywna = false; static uint32_t symulacjaWilgotnosciDo = 0;
static float symulowanaWilgotnosc = 55.0f;
static bool symulacjaTrwa(bool &aktywna, uint32_t &do_, const char*) {
  if (aktywna && g_ms >= do_) aktywna = false;
  return aktywna;
}
static float temperaturaNaZewnatrz = 2.0f, wilgotnoscSkorygowana = 0.0f, wilgotnoscOdczytana = 0.0f;
static int strzalkaWilgotnosc = 0;
static float obliczWilgotnoscAbsolutna(float, float rh) { return rh; }
static float obliczSkorygowanaWilgotnoscWzgledna(float abs, float) { return abs; }

// skryptowane odpowiedzi DHT22: kolejka ramek (err, t, rh); pusta kolejka = timeout
struct Ramka { uint8_t err; float t, rh; };
static std::vector<Ramka> g_ramki; static size_t g_ramkaIdx = 0; static int g_odczyty = 0;
static uint8_t read_dht(float &t, float &rh, uint8_t pin, uint8_t typ) {
  if (pin != DHT_PIN || typ != DHT22) { std::puts("  BLAD: read_dht zle argumenty"); return DHT_TIMEOUT; }
  ++g_odczyty;
  if (g_ramkaIdx >= g_ramki.size()) { t = NAN; rh = NAN; return DHT_TIMEOUT; }
  const Ramka &r = g_ramki[g_ramkaIdx++]; t = r.t; rh = r.rh; return r.err;
}

#include "wilgotnosc_wycinek.h"

// ── pomocnicze ─────────────────────────────────────────────────────────────
static int bledy = 0, sprawdzen = 0;
#define SPRAWDZ(w, opis) do { ++sprawdzen; if (!(w)) { ++bledy; std::printf("  BLAD [%d] %s\n", __LINE__, opis); } } while (0)
static bool rowne(float a, float b) { return std::fabs(a - b) < 1e-4f; }
static bool wLogu(const char* fragment) {
  for (auto &l : g_logi) if (l.find(fragment) != std::string::npos) return true;
  return false;
}
// biegnij: wywoluj wilgotnosc() co 3000 ms przez <ms> milisekund (jak scheduler centrali)
static void biegnij(uint32_t ms) {
  const uint32_t koniec = g_ms + ms;
  while (g_ms < koniec) { g_ms += 3000; wilgotnosc(); }
}
static void reset() {
  g_ms = 1000; g_logi.clear(); g_ramki.clear(); g_ramkaIdx = 0; g_odczyty = 0;
  g_telAttempt = g_telInvalid = g_telAccepted = g_telAcceptedSim = 0;
  g_zew2Filtr = Zew2Filtr{NAN, NAN, 0, false, 0, 0, 0, 0, false, 0, 0, 0};
  zapiszTempDiag(NAN); g_tempDhtAtUs = 0;
  dhtOstatniaProbaMs = 0; dhtBylaProba = false;
  symulacjaWilgotnosciAktywna = false; wilgotnoscOdczytana = 0; wilgotnoscSkorygowana = 0;
}
// UWAGA: bylaProbaCykliczna/ostatniaProbaCyklicznaMs sa static w wilgotnosc() -
// nie da sie ich wyzerowac; testy ida po kolei na jednym zegarze (g_ms rosnie).
static float zew2() { return czytajTempDiag(); }
static std::string zew2Json() { return jsonTempDiag().c_str(); }

int main() {
  reset();
  std::puts("[1] start: pierwszy odczyt od razu, potem co 120 s (nie co 3 s)");
  g_ramki = {{DHT_OK, 2.0f, 90.0f}, {DHT_OK, 2.2f, 91.0f}, {DHT_OK, 2.1f, 91.0f}};
  g_ms += 3000; wilgotnosc();
  SPRAWDZ(g_odczyty == 1 && rowne(zew2(), 2.0f), "pierwszy odczyt 2.0 na wykresie");
  SPRAWDZ(zew2Json() == "2.0", "jsonTempDiag() = 2.0");
  SPRAWDZ(rowne(wilgotnoscOdczytana, 90.0f), "wilgotnosc 90");
  biegnij(60000);
  SPRAWDZ(g_odczyty == 1, "po 60 s nadal jeden odczyt (bramka 120 s)");
  biegnij(63000);
  SPRAWDZ(g_odczyty == 2 && rowne(zew2(), 2.2f), "po 120 s drugi odczyt 2.2");
  SPRAWDZ(wLogu("DHT22 diagnostyka proby=2") && wLogu("zew2_przyjete=2"), "linia diagnostyki z licznikami zew2");

  std::puts("[2] pik -2.1 przy 2.1: nic na wykresie, powtorka po 3 s pokazuje 2.1");
  g_ramki.push_back({DHT_OK, -2.1f, 90.0f}); g_ramki.push_back({DHT_OK, 2.1f, 90.0f});
  biegnij(120000);                       // trzeci odczyt cykliczny (2.1) + ...
  const int przed = g_odczyty; g_logi.clear();
  biegnij(120000);                       // ... czwarty: -2.1 (podejrzany)
  // w tym oknie: odczyt -2.1, 3 s pozniej powtorka 2.1
  SPRAWDZ(g_odczyty == przed + 2, "podejrzany odczyt + powtorka = 2 odczyty w jednym cyklu");
  SPRAWDZ(rowne(zew2(), 2.1f), "na wykresie 2.1, pik nie przeszedl");
  SPRAWDZ(wLogu("zdarzenie=podejrzany t=-2.1 ostatnia=2.1") && wLogu("powtorka za 3 s"), "log podejrzany");
  SPRAWDZ(g_zew2Filtr.odrzucone == 1 && !g_zew2Filtr.weryfikacja, "1 odrzucony, weryfikacja zamknieta");
  { // powtorka byla dokladnie 3 s po podejrzanym odczycie (jedno wywolanie zadania)
    std::string pod, pow; for (auto &l : g_logi) { if (l.find("podejrzany") != std::string::npos) pod = l; }
    SPRAWDZ(!pod.empty(), "log podejrzany istnieje");
  }

  std::puts("[3] prawdziwy skok 2.1 -> 6.0: potwierdzony po 3 s, cykl 120 s nie przesuniety");
  g_ramki.push_back({DHT_OK, 6.0f, 85.0f}); g_ramki.push_back({DHT_OK, 6.2f, 85.0f}); g_ramki.push_back({DHT_OK, 6.1f, 85.0f});
  g_logi.clear(); const int przed3 = g_odczyty; const uint32_t t0 = g_ms;
  // dojdz do nastepnego odczytu cyklicznego
  while (g_odczyty == przed3) { g_ms += 3000; wilgotnosc(); }
  const uint32_t tOdczytu = g_ms;
  SPRAWDZ(std::isnan(zew2()) == false && rowne(zew2(), 2.1f), "zaraz po odczycie 6.0 wykres nadal 2.1");
  g_ms += 3000; wilgotnosc();
  SPRAWDZ(g_odczyty == przed3 + 2 && rowne(zew2(), 6.2f), "3 s pozniej powtorka 6.2 -> na wykresie 6.2");
  SPRAWDZ(wLogu("zdarzenie=potwierdzony t=6.2"), "log potwierdzony");
  const int przed3b = g_odczyty;
  while (g_odczyty == przed3b) { g_ms += 3000; wilgotnosc(); }
  SPRAWDZ(g_ms - tOdczytu == 120000, "nastepny cykliczny odczyt dokladnie 120 s po poprzednim cyklicznym (powtorka nie przesuwa cyklu)");
  SPRAWDZ(rowne(zew2(), 6.1f), "6.1");
  (void)t0;

  std::puts("[4] zla ramka (RH 3366.8 %): temperatura i wilgotnosc odrzucone, Zew 2 zostaje; [v3.58.10] ponowienie po ~6 s");
  g_ramki.push_back({DHT_OK, 6.1f, 3366.8f}); g_ramki.push_back({DHT_OK, 6.1f, 84.0f});
  g_logi.clear(); const int inv = g_telInvalid; const float rhPrzed = wilgotnoscOdczytana;
  const int przed4 = g_odczyty; while (g_odczyty == przed4) { g_ms += 3000; wilgotnosc(); }
  const uint32_t tCykl4 = g_ms;
  SPRAWDZ(rowne(zew2(), 6.1f), "Zew 2 nadal 6.1 (poprzednia wartosc, nie NaN)");
  SPRAWDZ(g_telInvalid == inv + 1 && rowne(wilgotnoscOdczytana, rhPrzed), "RH: telemetria INVALID, zostaje poprzednia");
  SPRAWDZ(wLogu("zdarzenie=zla_ramka t=6.1 rh=3366.8"), "log zla_ramka");
  SPRAWDZ(wLogu("DHT22 Zew2 ponowienie=1/2 za 5 s kod=0"), "log ponowienie 1/2");
  g_ms += 3000; wilgotnosc();
  SPRAWDZ(g_odczyty == przed4 + 1, "3 s po zlej ramce jeszcze nic (ponowienie po 5 s, nie powtorka po 3 s)");
  g_ms += 3000; wilgotnosc();
  SPRAWDZ(g_odczyty == przed4 + 2 && rowne(zew2(), 6.1f) && rowne(wilgotnoscOdczytana, 84.0f), "6 s po zlej ramce ponowienie: Zew 2 6.1, RH 84");
  SPRAWDZ(!g_zew2Filtr.ponowienie && g_zew2Filtr.ponowienia == 1, "po dobrym ponowieniu koniec");
  { const int p = g_odczyty; while (g_odczyty == p) { g_ms += 3000; wilgotnosc(); } }
  SPRAWDZ(g_ms - tCykl4 == 120000, "nastepny odczyt cykliczny 120 s po poprzednim cyklicznym (ponowienie nie przesuwa cyklu)");
  // ten odczyt: kolejka pusta -> timeout -> ponowienia 1/2 i 2/2, potem cisza do nastepnego cyklu
  SPRAWDZ(rowne(zew2(), 6.1f), "po timeout Zew 2 zostaje 6.1");
  g_logi.clear(); const int p4b = g_odczyty; const uint32_t tCykl4b = g_ms;
  biegnij(30000);
  SPRAWDZ(g_odczyty == p4b + 2 && wLogu("ponowienie=2/2 za 5 s kod=2"), "po timeout: dokladnie 2 ponowienia (6 s i 12 s), potem limit");
  SPRAWDZ(!g_zew2Filtr.ponowienie, "trzeciego ponowienia nie ma");
  { const int p = g_odczyty; while (g_odczyty == p) { g_ms += 3000; wilgotnosc(); } }
  SPRAWDZ(g_ms - tCykl4b == 120000, "po 2 nieudanych ponowieniach nastepna proba w zwyklym cyklu 120 s");

  std::puts("[5] brak odczytow (timeouty): Zew 2 gasnie po 255 s od ostatniej dobrej wartosci (bylo 135 s)");
  // kolejka pusta -> timeouty (3 proby na cykl); ostatnia dobra 6.1 byla w [4] (ponowienie)
  g_logi.clear();
  { const uint64_t wiek = ((uint64_t)g_ms * 1000ULL - g_tempDhtAtUs) / 1000ULL;
    SPRAWDZ(wiek > 120000 && wiek < 255000 && rowne(zew2(), 6.1f), "po jednym nieudanym cyklu (wiek 120-255 s) Zew 2 nadal 6.1 - tu 3.58.9 mialo juz null"); }
  biegnij(120000);
  SPRAWDZ(std::isnan(zew2()), "po >255 s bez dobrej ramki Zew 2 = null");
  SPRAWDZ(zew2Json() == "null", "jsonTempDiag() = null");
  SPRAWDZ(!wLogu("zdarzenie=zla_ramka"), "timeout biblioteki nie jest logowany jako zla_ramka (kod != DHT_OK)");

  std::puts("[6] po przerwie pierwsza dobra ramka wraca od razu (historia 6.1 -> 3.0 to skok > 3: powtorka)");
  g_ramki.push_back({DHT_OK, 3.0f, 92.0f}); g_ramki.push_back({DHT_OK, 3.0f, 92.0f});
  const int przed6 = g_odczyty; while (g_odczyty == przed6) { g_ms += 3000; wilgotnosc(); }
  SPRAWDZ(std::isnan(zew2()), "3.0 vs ostatnia przyjeta 6.1 -> podejrzane, jeszcze nic");
  g_ms += 3000; wilgotnosc();
  SPRAWDZ(rowne(zew2(), 3.0f), "powtorka potwierdza 3.0");

  std::puts("[7] symulacja wilgotnosci: Zew 2 = null, oczekujaca powtorka skasowana, bez odczytu DHT");
  g_ramki.push_back({DHT_OK, -3.0f, 92.0f});   // pik -> weryfikacja
  const int przed7 = g_odczyty; while (g_odczyty == przed7) { g_ms += 3000; wilgotnosc(); }
  SPRAWDZ(g_zew2Filtr.weryfikacja, "weryfikacja oczekuje");
  symulacjaWilgotnosciAktywna = true; symulacjaWilgotnosciDo = g_ms + 600000;
  g_ms += 3000; wilgotnosc();                    // powtorka omija bramke 120 s -> trafia na symulacje
  SPRAWDZ(g_odczyty == przed7 + 1, "w symulacji read_dht nie jest wolane");
  SPRAWDZ(std::isnan(zew2()) && !g_zew2Filtr.weryfikacja && !g_zew2Filtr.ponowienie, "Zew 2 null, weryfikacja i ponowienie skasowane");
  SPRAWDZ(rowne(wilgotnoscOdczytana, 55.0f) && g_telAcceptedSim >= 1, "wilgotnosc symulowana 55");
  symulacjaWilgotnosciAktywna = false;

  std::puts("[8] przepelnienie millis(): powtorka i cykl 120 s dzialaja przez granice 2^32");
  g_ramki.push_back({DHT_OK, 3.1f, 90.0f}); g_ramki.push_back({DHT_OK, -3.1f, 90.0f}); g_ramki.push_back({DHT_OK, 3.1f, 90.0f}); g_ramki.push_back({DHT_OK, 3.2f, 90.0f});
  const int przed8 = g_odczyty; while (g_odczyty == przed8) { g_ms += 3000; wilgotnosc(); }   // 3.1 przyjete (po symulacji)
  SPRAWDZ(rowne(zew2(), 3.1f), "3.1 po symulacji (historia 3.0, bez powtorki)");
  g_ms = 4294967295UL - 1000;                    // 1 s przed przepelnieniem - wymus odczyt cykliczny
  g_ms += 3000; wilgotnosc();                    // tu millis juz po przepelnieniu (1999)
  // odczyt -3.1: podejrzany
  if (g_odczyty == przed8 + 1) { g_ms += 3000; wilgotnosc(); }
  SPRAWDZ(g_odczyty >= przed8 + 2, "odczyt po przepelnieniu sie odbyl");
  g_ms += 3000; wilgotnosc();
  SPRAWDZ(rowne(zew2(), 3.1f) || rowne(zew2(), 3.2f), "po powtorce prawdziwa wartosc, nie -3.1");
  SPRAWDZ(!wLogu("t=-3.1 ostatnia=-3.1"), "-3.1 nigdy nie zostalo 'ostatnia dobra'");

  std::puts("[9] [v3.58.10] odrzucony po powtorce -> ponowienie po 5 s daje prawde; nic nie znika z wykresu");
  g_ramkaIdx = g_ramki.size();                                     // resztki kolejki z [8] precz
  g_ramki.push_back({DHT_OK, 9.0f, 90.0f}); g_ramki.push_back({DHT_OK, 14.0f, 90.0f}); g_ramki.push_back({DHT_OK, 3.3f, 90.0f});
  g_logi.clear(); const int przed9 = g_odczyty; const float zewPrzed9 = zew2();
  while (g_odczyty == przed9) { g_ms += 3000; wilgotnosc(); }      // 9.0: podejrzany
  g_ms += 3000; wilgotnosc();                                      // 14.0: odrzucony (3 s)
  SPRAWDZ(g_odczyty == przed9 + 2 && wLogu("zdarzenie=odrzucony") && wLogu("ponowienie=1/2 za 5 s kod=0"), "odrzucony -> ponowienie");
  SPRAWDZ(rowne(zew2(), zewPrzed9), "na wykresie nadal poprzednia wartosc");
  g_ms += 3000; wilgotnosc();                                      // 3 s: jeszcze nie
  SPRAWDZ(g_odczyty == przed9 + 2, "3 s po odrzuceniu jeszcze nic");
  g_ms += 3000; wilgotnosc();                                      // 6 s: ponowienie 3.3
  SPRAWDZ(g_odczyty == przed9 + 3 && rowne(zew2(), 3.3f) && !g_zew2Filtr.ponowienie, "ponowienie 3.3 przyjete");
  SPRAWDZ(wLogu("zew2_ponowienia="), "licznik ponowien w linii diagnostyki");


  std::puts("[10] [v3.58.20] klon U2 (26.11): temperatura schodzi ponizej 0 - Zew 2 i wilgotnosc dalej przyjmowane, nie BRAK DANYCH");
  // biblioteka dhtESP32-rmt dla klona U2: -0,1 C -> -3276.7, -0,3 -> -3276.5, -1,2 -> -3275.6 (znak-modul wg karty)
  g_ramkaIdx = g_ramki.size();
  g_ramki.push_back({DHT_OK, 0.0f, 90.1f});       // ostatnia dobra z [9] to 3.3 -> 0.0 to skok 3.3 > 3 -> powtorka
  g_ramki.push_back({DHT_OK, 0.0f, 90.1f});
  g_ramki.push_back({DHT_OK, -3276.7f, 90.3f});   // -0.1
  g_ramki.push_back({DHT_OK, -3276.5f, 90.5f});   // -0.3
  g_ramki.push_back({DHT_OK, -3275.6f, 91.0f});   // -1.2
  g_logi.clear();
  { const int p = g_odczyty; while (g_odczyty == p) { g_ms += 3000; wilgotnosc(); } }   // 0.0 podejrzany
  g_ms += 3000; wilgotnosc();                                                           // powtorka 0.0
  SPRAWDZ(rowne(zew2(), 0.0f) && rowne(wilgotnoscOdczytana, 90.1f), "0.0 potwierdzone, RH 90.1");
  const int acc10 = g_telAccepted, inv10 = g_telInvalid;
  { const int p = g_odczyty; while (g_odczyty == p) { g_ms += 3000; wilgotnosc(); } }   // -3276.7 -> -0.1
  SPRAWDZ(rowne(zew2(), -0.1f), "ramka -3276.7 z biblioteki = -0.1 C na Zew 2 (do 3.58.19: zla ramka, po 255 s null)");
  SPRAWDZ(rowne(wilgotnoscOdczytana, 90.3f) && g_telAccepted == acc10 + 1 && g_telInvalid == inv10, "wilgotnosc z tej ramki przyjeta (telemetria ACCEPTED, nie INVALID)");
  SPRAWDZ(wLogu("zdarzenie=u2 surowa=-3276.7 t=-0.1"), "WARN o klonie U2 przy pierwszym wykryciu");
  SPRAWDZ(!wLogu("zdarzenie=zla_ramka"), "bez logu zla_ramka");
  SPRAWDZ(!g_zew2Filtr.ponowienie, "bez ponowienia (ramka dobra)");
  SPRAWDZ(zew2Json() == "-0.1", "jsonTempDiag() = -0.1 (/api/status, Firebase)");
  { const int p = g_odczyty; while (g_odczyty == p) { g_ms += 3000; wilgotnosc(); } }   // -0.3
  SPRAWDZ(rowne(zew2(), -0.3f) && rowne(wilgotnoscOdczytana, 90.5f), "-0.3 / RH 90.5");
  g_logi.clear();
  { const int p = g_odczyty; while (g_odczyty == p) { g_ms += 3000; wilgotnosc(); } }   // -1.2
  SPRAWDZ(rowne(zew2(), -1.2f) && rowne(wilgotnoscOdczytana, 91.0f), "-1.2 / RH 91.0");
  SPRAWDZ(!wLogu("zdarzenie=u2"), "WARN o U2 tylko raz (potem cicho)");
  SPRAWDZ(wLogu("u2=3 kod=0"), "licznik u2=3 w linii 'DHT22 diagnostyka'");
  SPRAWDZ(g_telInvalid == inv10, "zadna z ramek ujemnych nie poszla jako INVALID");

  std::puts("[11] [v3.58.20] smieci nadal odpadaja: -3236.7 (obraz -40.1), -3276.8, +3276.7, U2 z RH 3366.8");
  g_ramki.push_back({DHT_OK, -3236.7f, 91.0f});
  g_ramki.push_back({DHT_OK, -3276.8f, 91.0f});
  g_ramki.push_back({DHT_OK, 3276.7f, 91.0f});
  g_ramki.push_back({DHT_OK, -3276.5f, 3366.8f});
  g_logi.clear(); const int inv11 = g_telInvalid;
  // kazda proba: odczyt cykliczny + 2 ponowienia (kolejka daje kolejne smieci / potem timeouty)
  { const int p = g_odczyty; while (g_odczyty < p + 4) { g_ms += 3000; wilgotnosc(); } }
  SPRAWDZ(g_telInvalid == inv11 + 4, "4 smieciowe ramki: 4x INVALID");
  SPRAWDZ(wilgotnoscOdczytana >= 1.0f && wilgotnoscOdczytana <= 100.0f, "RH nie wziela 3366.8 (zostaje bufor poprzedniej wartosci - logika jak dotad)");
  SPRAWDZ(wLogu("zla_ramka t=-3236.7") && wLogu("zla_ramka t=-3276.8") && wLogu("zla_ramka t=3276.7") && wLogu("zla_ramka t=-0.3 rh=3366.8"), "logi zla_ramka (ostatnia: temperatura odkodowana, ale RH dyskwalifikuje ramke)");
  SPRAWDZ(rowne(zew2(), -1.2f), "Zew 2 zostaje -1.2 (poprzednia dobra)");

  std::printf("\nWILGOTNOSC(): %d sprawdzen, %d bledow, %d odczytow DHT, %zu linii logu\n", sprawdzen, bledy, g_odczyty, g_logi.size());
  return bledy ? 1 : 0;
}
