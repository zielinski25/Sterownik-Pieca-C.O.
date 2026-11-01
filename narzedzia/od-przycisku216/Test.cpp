// 216: test "od przycisku do pliku" - PRAWDZIWE adresy z panelu podane
// PRAWDZIWEJ trasie z firmware.
//
// Co jest prawdziwe:
//   * naglowek HistoriaPobranie.h v3.56.3 (trasa + Content-Disposition),
//   * HistoriaBilety.h (bilety), zapinka 211, bramka i obiekty z main_centrala.cpp
//     po patchu v3.56.3 (Extracted216.inc),
//   * tresci trzech lambd tras (/history, /history/long, /history/link),
//   * tabela tras z pliku po patchu (Trasy216.inc),
//   * ADRESY: nie sa tu wpisane recznie - pochodza z panel_test.mjs, ktory
//     uruchomil prawdziwy skrypt panelu w silniku JS i wypisal, pod jaki adres
//     panel naprawde prosi o bilet i pod jaki potem przechodzi.
//
// Modelowane jest tylko to, czego firmware nie ma na komputerze: serwer HTTP,
// VFS w RAM, semafory, esp_random i zegar.
//
// Uruchomienie (adresy z panelu w pierwszym argumencie):
//   ./test216 <katalog>/urls.txt
// Wynik: "PASS trasa216 sprawdzen=N bledow=0" i kod 0, albo FAIL i kod 1.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "Arduino.h"
#include "RamCalls.h"
#include "HistoriaPin.h"
#include "Serwer213.h"
#include "HistoriaPobranie.h"
#include "Model213.h"

// --- globalne obiekty centrali, ktorych dotyka bramka --------------------
bool littlefsReady = true;
volatile size_t g_histShortSize = 0;
volatile size_t g_histLongSize = 0;
SemaphoreHandle_t logMutex = nullptr;
RamCalls g_ram216;
history193::Calls &historiaSys = g_ram216;
bool g_historiaGotowa216 = true;
const char *HISTORIA_TOKEN = "token-testowy-0123456789";
unsigned long g_zegar211 = 0;
std::uint32_t g_los213 = 1;
int g_psmallocFail211 = 0;
int g_mallocFail211 = 0;
int g_diagErr211 = 0;
int g_diagWarn211 = 0;
bool g_blokadaZajeta211 = false;
WiFiClass WiFi;
SerialClass Serial;

bool historiaDostepGotowy() { return g_historiaGotowa216; }

namespace {
struct Sem {
  std::timed_mutex m;
};
}

SemaphoreHandle_t xSemaphoreCreateMutex() { return new Sem(); }
int xSemaphoreTake(SemaphoreHandle_t h, unsigned ticks) {
  if (!h) return pdFALSE;
  if (g_blokadaZajeta211) return pdFALSE;
  Sem *s = static_cast<Sem *>(h);
  if (ticks == 0) return s->m.try_lock() ? pdTRUE : pdFALSE;
  return s->m.try_lock_for(std::chrono::milliseconds(ticks)) ? pdTRUE : pdFALSE;
}
int xSemaphoreGive(SemaphoreHandle_t h) {
  if (!h) return pdFALSE;
  static_cast<Sem *>(h)->m.unlock();
  return pdTRUE;
}

void *model_ps_malloc(size_t n) { return g_psmallocFail211 ? nullptr : std::malloc(n); }
void *model_malloc(size_t n) { return g_mallocFail211 ? nullptr : std::malloc(n); }
void SerialClass::printf(const char *, ...) {}
void SerialClass::println(const char *) {}
void diagEmit(int, int, const char *) {}

// --- PRAWDZIWY kod z pliku po patchu v3.56.3 ----------------------------
#include "Extracted216.inc"   // adaptery + tresci lambd + tabela trasa->obsluga
#include "Trasy216.inc"       // wszystkie trasy webserwera z firmware

// --- ramka testowa -------------------------------------------------------
namespace {
int g_grupa = 0;
int g_sprawdzen = 0;
int g_bledow = 0;
const char *g_nazwa = "";

void grupa(const char *n) {
  g_nazwa = n;
  ++g_grupa;
  std::printf("-- %02d %s\n", g_grupa, n);
}
void sprawdz(bool w, const char *co, int linia) {
  ++g_sprawdzen;
  if (!w) {
    ++g_bledow;
    std::printf("FAIL [%s] %s (Test.cpp:%d)\n", g_nazwa, co, linia);
  }
}
#define OK(x) sprawdz((x), #x, __LINE__)

historia213::Bilety bilety(historiaLosuj213, historiaZegar213);

void przygotuj() {
  g_ram216.wyczysc();
  g_historiaGotowa216 = true;
  littlefsReady = true;
  g_histShortSize = 0;
  g_histLongSize = 0;
  g_blokadaZajeta211 = false;
  g_zegar211 = 1000;
  g_los213 = 1;
  bilety.wyczysc();
}

using Obsluz = void (*)(AsyncWebServerRequest *);

const char *sciezkaAdresu(const std::string &adres) {
  static std::string bufor;
  const std::size_t i = adres.find('?');
  bufor = (i == std::string::npos) ? adres : adres.substr(0, i);
  return bufor.c_str();
}

// Parametry zapytania: "?a=1&b=2" -> lista par (bez dekodowania %XX: adresy
// z panelu maja tylko male litery i cyfry, patrz sprawdzenie nizej).
std::vector<std::pair<std::string, std::string>> parametry(const std::string &adres) {
  std::vector<std::pair<std::string, std::string>> wynik;
  const std::size_t i = adres.find('?');
  if (i == std::string::npos) return wynik;
  std::string reszta = adres.substr(i + 1);
  std::size_t od = 0;
  while (od <= reszta.size()) {
    const std::size_t koniec = reszta.find('&', od);
    const std::string para = reszta.substr(od, koniec == std::string::npos ? std::string::npos : koniec - od);
    if (!para.empty()) {
      const std::size_t rowna = para.find('=');
      if (rowna == std::string::npos) {
        wynik.emplace_back(para, "");
      } else {
        wynik.emplace_back(para.substr(0, rowna), para.substr(rowna + 1));
      }
    }
    if (koniec == std::string::npos) break;
    od = koniec + 1;
  }
  return wynik;
}

bool trasaZarejestrowana(const std::string &s) {
  for (int i = 0; i < TRASY216_N; ++i) {
    if (s == TRASY216[i]) return true;
  }
  return false;
}

Obsluz obslugaDla(const std::string &s) {
  for (int i = 0; i < TRASY_OBSLUGI216_N; ++i) {
    if (s == TRASY_OBSLUGI216[i].sciezka) return TRASY_OBSLUGI216[i].obsluz;
  }
  return nullptr;
}

bool maNaglowek(AsyncWebServerResponse *r, const char *n, const char *v) {
  if (r == nullptr) return false;
  for (const auto &h : r->naglowki) {
    if (h.first == n && h.second == v) return true;
  }
  return false;
}

std::string naglowek(AsyncWebServerResponse *r, const char *n) {
  if (r == nullptr) return std::string();
  for (const auto &h : r->naglowki) {
    if (h.first == n) return h.second;
  }
  return std::string();
}

// Jedno zadanie HTTP do PRAWDZIWEJ trasy. Parametry bierzemy z adresu, ktory
// wyszedl z panelu; "{BILET}" podmieniamy na bilet wydany przez centrale.
struct Zadanie {
  int kod = 0;
  AsyncWebServerResponse *odpowiedz = nullptr;
  std::string cialo;
};

Zadanie wykonaj(const std::string &adres, const std::string &bilet) {
  Zadanie z;
  const std::string sciezka = sciezkaAdresu(adres);
  Obsluz fn = obslugaDla(sciezka);
  if (fn == nullptr) return z;   // kod 0 = nie ma takiej trasy
  AsyncWebServerRequest r;
  r.metoda = HTTP_GET;
  for (const auto &kv : parametry(adres)) {
    std::string wartosc = kv.second;
    const std::string znacznik = "{BILET}";
    const std::size_t i = wartosc.find(znacznik);
    if (i != std::string::npos) {
      wartosc = wartosc.substr(0, i) + bilet + wartosc.substr(i + znacznik.size());
    }
    r.dodajParametr(kv.first.c_str(), wartosc.c_str());
  }
  fn(&r);
  z.kod = r.kod;
  z.odpowiedz = r.wyslana;
  z.cialo = r.beginCialo;
  return z;
}

bool wszystkieHex(const std::string &s) {
  if (s.size() != historia213::kZnakow) return false;
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

struct Para {
  std::string strumien;
  std::string link;
  std::string pobranie;
};
}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::printf("FAIL trasa216 brak argumentu: sciezka do urls.txt z panelu\n");
    return 2;
  }
  std::ifstream we(argv[1]);
  if (!we) {
    std::printf("FAIL trasa216 nie moge otworzyc %s\n", argv[1]);
    return 2;
  }

  std::vector<Para> pary;
  std::string linia;
  while (std::getline(we, linia)) {
    while (!linia.empty() && (linia.back() == '\n' || linia.back() == '\r')) linia.pop_back();
    if (linia.empty()) continue;
    const std::size_t a = linia.find('\t');
    const std::size_t b = linia.find('\t', a == std::string::npos ? 0 : a + 1);
    if (a == std::string::npos || b == std::string::npos) {
      std::printf("FAIL trasa216 zly wiersz w %s: %s\n", argv[1], linia.c_str());
      return 2;
    }
    pary.push_back({linia.substr(0, a), linia.substr(a + 1, b - a - 1), linia.substr(b + 1)});
  }
  if (pary.size() != 2) {
    std::printf("FAIL trasa216 oczekiwano 2 wierszy (krotka, dluga), jest %zu\n", pary.size());
    return 2;
  }
  std::printf("adresy z panelu: %s\n", argv[1]);

  logMutex = xSemaphoreCreateMutex();
  historiaPobranie.ustawBilety(&bilety);

  const std::string trescKrotka = "czas,temp\n1,21.5\n2,22.0\n";
  const std::string trescDluga = "czas,temp\n1,19.0\n2,19.5\n3,20.0\n4,20.5\n";

  for (const Para &p : pary) {
    const bool dluga = (p.strumien == "long");
    char tytul[128];
    std::snprintf(tytul, sizeof(tytul), "%s z panelu: %s -> %s",
                  p.strumien.c_str(), p.link.c_str(), p.pobranie.c_str());
    grupa(tytul);

    przygotuj();
    g_ram216.utworz("/littlefs/hist_short.csv", trescKrotka);
    g_ram216.utworz("/littlefs/hist_long.csv", trescDluga);
    g_histShortSize = trescKrotka.size();
    g_histLongSize = trescDluga.size();

    // 1. trasy, ktore podaje panel, musza byc zarejestrowane w firmware
    const std::string sciezkaLink = sciezkaAdresu(p.link);
    const std::string sciezkaPobrania = sciezkaAdresu(p.pobranie);
    OK(trasaZarejestrowana(sciezkaLink));
    OK(trasaZarejestrowana(sciezkaPobrania));
    OK(obslugaDla(sciezkaPobrania) != nullptr);

    // 2. bilet: dokladnie ten adres, ktory podaje panel
    Zadanie l = wykonaj(p.link, "");
    OK(l.kod == 200);
    OK(wszystkieHex(l.cialo));
    OK(maNaglowek(l.odpowiedz, "Cache-Control", "no-store"));
    const std::string bilet = l.cialo;
    delete l.odpowiedz;

    // 3. pobranie: dokladnie ten adres, ktory podaje panel
    Zadanie d = wykonaj(p.pobranie, bilet);
    OK(d.kod == 200);
    AsyncAbstractResponse *strumien = d.odpowiedz ? d.odpowiedz->jakoStrumien() : nullptr;
    OK(strumien != nullptr);
    if (strumien != nullptr) {
      Wysylka w = wyslij(strumien, 97);
      const std::string &oczekiwana = dluga ? trescDluga : trescKrotka;
      const std::string &nieoczekiwana = dluga ? trescKrotka : trescDluga;
      OK(w.cialo == oczekiwana);
      OK(w.cialo != nieoczekiwana);   // dowod, ze poszedl wlasciwy plik

      // 4. plik musi sie dac zapisac, a nie tylko wyswietlic (D3)
      const std::string disp = naglowek(d.odpowiedz, "Content-Disposition");
      OK(disp == (dluga ? "attachment; filename=\"historia-dluga.csv\""
                        : "attachment; filename=\"historia-krotka.csv\""));
      OK(d.odpowiedz->naglowki.size() >= 1);
    }
    delete d.odpowiedz;
    OK(!historiaPobranie.zajeta());

    // 5. ten sam bilet drugi raz nie dziala (link jest jednorazowy)
    Zadanie d2 = wykonaj(p.pobranie, bilet);
    OK(d2.kod == 401);
    delete d2.odpowiedz;
  }

  std::printf("%s trasa216 sprawdzen=%d bledow=%d\n",
              g_bledow == 0 ? "PASS" : "FAIL", g_sprawdzen, g_bledow);
  return g_bledow == 0 ? 0 : 1;
}
