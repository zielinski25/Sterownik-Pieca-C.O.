// 218: test hostowy CALEGO STYKU "panel -> tabela tras -> pobranie".
//
// Dlaczego ten etap istnieje: etapy 212/213 sprawdzaly trase i panel, ale
// trase wolaly BEZPOSREDNIO (`historiaPobranie.obsluz(...)`), z pominieciem
// tabeli tras serwera. Dlatego nie zauwazyly, ze `/history` zarejestrowane
// gołym napisem (matcher BackwardCompatible) przechwytuje `/history/long`
// i `/history/link`, bo serwer bierze PIERWSZY pasujacy handler.
//
// Co jest PRAWDZIWE w tym tescie:
//   * tabela tras: tekst trzech rejestracji wyciagniety z main_centrala.cpp
//     v3.56.4 (Extracted218.inc) - kompiluje sie doslownie,
//   * naglowek trasy (HistoriaPobranie.h v3.56.3) i biletow (HistoriaBilety.h),
//   * bramka i obiekt trasy z etapu 212 (Extracted212.inc),
//   * adresy, ktore panel naprawde generuje - czytane z pliku zapisanego
//     przez panel218.mjs (argv[1]), nie przepisane recznie.
// Modelowane jest tylko to, czego firmware nie ma na komputerze: serwer HTTP
// (z zachowaniem matcherow z WebServer.cpp), VFS w RAM, semafory, esp_random.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "Arduino.h"
#include "RamCalls.h"
#include "HistoriaPin.h"
#include "Serwer218.h"
#include "HistoriaPobranie.h"

// --- globalne obiekty centrali, ktorych dotyka bramka ---------------------
bool littlefsReady = true;
volatile size_t g_histShortSize = 0;
volatile size_t g_histLongSize = 0;
SemaphoreHandle_t logMutex = nullptr;
RamCalls g_ram218;
history193::Calls &historiaSys = g_ram218;
bool g_historiaGotowa218 = true;
const char *HISTORIA_TOKEN = "token-testowy-0123456789";
unsigned long g_zegar211 = 0;
int g_psmallocFail211 = 0;
int g_mallocFail211 = 0;
WiFiClass WiFi;
SerialClass Serial;

bool historiaDostepGotowy() { return g_historiaGotowa218; }

namespace {
struct Sem {
  std::timed_mutex m;
};
}

SemaphoreHandle_t xSemaphoreCreateMutex() { return new Sem(); }
int xSemaphoreTake(SemaphoreHandle_t h, unsigned ticks) {
  if (!h) return pdFALSE;
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

// esp_random() z ESP-IDF - w tescie deterministyczny LCG (tak jak w etapie 213).
std::uint32_t g_los218 = 1;
std::uint32_t esp_random() {
  g_los218 = g_los218 * 1664525u + 1013904223u;
  return g_los218;
}

// --- PRAWDZIWY kod z etapu 212: bramka + obiekt trasy ---------------------
#include "../pobranie-trasa212/Extracted212.inc"

// --- PRAWDZIWY serwer: tekst rejestracji z v3.56.4 ------------------------
AsyncWebServer wifiWebServer;
#include "Extracted218.inc"

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

// --- rozbior adresu tak, jak robi to WebRequest.cpp ----------------------
std::string urlDecode218(const std::string &t) {
  std::string w;
  w.reserve(t.size());
  for (std::size_t i = 0; i < t.size(); ++i) {
    const char c = t[i];
    if (c == '+') {
      w += ' ';
    } else if (c == '%' && i + 2 < t.size()) {
      auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        return -1;
      };
      const int a = hex(t[i + 1]), b = hex(t[i + 2]);
      if (a >= 0 && b >= 0) {
        w += static_cast<char>(a * 16 + b);
        i += 2;
      } else {
        w += c;
      }
    } else {
      w += c;
    }
  }
  return w;
}

struct Adres218 {
  std::string sciezka;
  std::vector<std::pair<std::string, std::string>> parametry;
};

Adres218 rozbierz218(const std::string &url) {
  Adres218 a;
  const std::size_t pytanie = url.find('?');
  a.sciezka = urlDecode218(url.substr(0, pytanie));
  if (pytanie == std::string::npos) return a;
  std::size_t i = pytanie + 1;
  while (i <= url.size()) {
    std::size_t koniec = url.find('&', i);
    if (koniec == std::string::npos) koniec = url.size();
    const std::string para = url.substr(i, koniec - i);
    const std::size_t rowna = para.find('=');
    const std::string nazwa = rowna == std::string::npos ? para : para.substr(0, rowna);
    const std::string wartosc = rowna == std::string::npos ? std::string() : para.substr(rowna + 1);
    if (!nazwa.empty()) a.parametry.emplace_back(urlDecode218(nazwa), urlDecode218(wartosc));
    if (koniec == url.size()) break;
    i = koniec + 1;
  }
  return a;
}

struct Wynik218 {
  int kod = 0;
  int trasa = -1;   // indeks trasy w tabeli, -1 = onNotFound
  AsyncWebServerResponse *odpowiedz = nullptr;
  std::string cialo;
};

// Wyslanie zadania do PRAWDZIWEJ tabeli tras (ta sama droga co w serwerze).
Wynik218 wyslij218(const std::string &url, int metoda = HTTP_GET, const char *auth = nullptr) {
  AsyncWebServerRequest r;
  r.metoda = metoda;
  if (auth != nullptr) r.dodajNaglowek("Authorization", auth);
  const Adres218 a = rozbierz218(url);
  for (const auto &kv : a.parametry) r.dodajParametr(kv.first.c_str(), kv.second.c_str());
  const int indeks = wifiWebServer.obsluz(&r, String(a.sciezka.c_str()));
  Wynik218 w;
  w.kod = r.kod;
  w.trasa = indeks;
  w.odpowiedz = r.wyslana;
  w.cialo = r.beginCialo;
  return w;
}

bool maNaglowek218(AsyncWebServerResponse *r, const char *n, const char *v) {
  if (r == nullptr) return false;
  for (const auto &h : r->naglowki) {
    if (h.first == n && h.second == v) return true;
  }
  return false;
}

bool wszystkieHex218(const std::string &s) {
  if (s.size() != historia213::kZnakow) return false;
  for (char c : s) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!ok) return false;
  }
  return true;
}

struct Odebrane218 {
  std::string cialo;
  bool zrodloWazne = false;
  std::size_t porcji = 0;
};

Odebrane218 odbierz218(AsyncWebServerResponse *r, std::size_t bufor = 101) {
  Odebrane218 w;
  AsyncAbstractResponse *strumien = r != nullptr ? r->jakoStrumien() : nullptr;
  if (strumien == nullptr) return w;
  std::vector<uint8_t> b(bufor);
  for (int i = 0; i < 1000000; ++i) {
    const std::size_t n = strumien->_fillBuffer(b.data(), b.size());
    if (n == 0) break;
    w.cialo.append(reinterpret_cast<const char *>(b.data()), n);
    ++w.porcji;
  }
  w.zrodloWazne = strumien->_sourceValid();
  return w;
}

void przygotuj() {
  g_ram218.wyczysc();
  g_historiaGotowa218 = true;
  littlefsReady = true;
  g_histShortSize = 0;
  g_histLongSize = 0;
  g_zegar211 = 1000;
  g_los218 = 1;
  g_psmallocFail211 = 0;
  g_mallocFail211 = 0;
  historiaBilety.wyczysc();
}

// Plik testowy wiekszy niz bufor odpowiedzi (97-101 B), zeby miec kilka porcji.
std::string zrobCsv(const char *prefiks, int wierszy, std::size_t szerokosc) {
  std::string w;
  for (int i = 0; i < wierszy; ++i) {
    w += prefiks;
    w += ",";
    w += std::to_string(i);
    w += ",";
    w += std::string(szerokosc, static_cast<char>('a' + (i % 26)));
    w += "\n";
  }
  return w;
}

std::vector<std::string> wczytajTsv(const char *sciezka, std::size_t oczekiwane_kolumny) {
  std::vector<std::string> linie;
  std::FILE *f = std::fopen(sciezka, "rb");
  if (f == nullptr) return linie;
  char bufor[4096];
  while (std::fgets(bufor, sizeof(bufor), f) != nullptr) {
    std::string l(bufor);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    if (l.empty()) continue;
    std::size_t pol = 1;
    for (char c : l) {
      if (c == '\t') ++pol;
    }
    if (pol != oczekiwane_kolumny) continue;
    linie.push_back(l);
  }
  std::fclose(f);
  return linie;
}

std::vector<std::string> podziel(const std::string &l) {
  std::vector<std::string> p;
  std::size_t i = 0;
  for (;;) {
    const std::size_t t = l.find('\t', i);
    if (t == std::string::npos) {
      p.push_back(l.substr(i));
      break;
    }
    p.push_back(l.substr(i, t - i));
    i = t + 1;
  }
  return p;
}
}  // namespace

int main(int argc, char **argv) {
  logMutex = xSemaphoreCreateMutex();
  zarejestrujTrasy218();

  // ---- 01: tabela tras po patchu --------------------------------------
  grupa("tabela tras: trzy trasy /history* przez exact");
  {
    OK(wifiWebServer.liczbaTras() == 3);
    OK(wifiWebServer.trasa(0).matcher.typ() == AsyncURIMatcher::EXACT);
    OK(wifiWebServer.trasa(1).matcher.typ() == AsyncURIMatcher::EXACT);
    OK(wifiWebServer.trasa(2).matcher.typ() == AsyncURIMatcher::EXACT);
    OK(wifiWebServer.trasa(0).matcher.wartosc() == "/history");
    OK(wifiWebServer.trasa(1).matcher.wartosc() == "/history/long");
    OK(wifiWebServer.trasa(2).matcher.wartosc() == "/history/link");
    OK(wifiWebServer.trasa(0).metoda == HTTP_ANY);
    OK(wifiWebServer.trasa(1).metoda == HTTP_ANY);
    OK(wifiWebServer.trasa(2).metoda == HTTP_GET);
    // Kolejnosc rejestracji jest teraz bez znaczenia - to jest cala poprawka.
    OK(wifiWebServer.dopasuj("/history", HTTP_GET) == 0);
    OK(wifiWebServer.dopasuj("/history/long", HTTP_GET) == 1);
    OK(wifiWebServer.dopasuj("/history/link", HTTP_GET) == 2);
    // Adresy, ktore nie sa zadna z tras, nie moga byc lykane.
    OK(wifiWebServer.dopasuj("/history/long/x", HTTP_GET) == -1);
    OK(wifiWebServer.dopasuj("/historyXXX", HTTP_GET) == -1);
    OK(wifiWebServer.dopasuj("/histor", HTTP_GET) == -1);
  }

  // ---- 02: DOWOD, ze stara tabela byla zepsuta -------------------------
  // Ten sam serwer, te same trasy, ale zarejestrowane tak jak w v3.56.3:
  // gołym napisem. Test ma pokazac, ze wtedy /history/link i /history/long
  // NIE sa osiagalne - to jest wlasnie blad, ktory naprawia ten etap.
  grupa("stara tabela (goly napis) przechwytuje /history/link i /history/long");
  {
    AsyncWebServer stary;
    stary.on("/history", HTTP_ANY, [](AsyncWebServerRequest *) {});
    stary.on("/history/long", HTTP_ANY, [](AsyncWebServerRequest *) {});
    stary.on("/history/link", HTTP_GET, [](AsyncWebServerRequest *) {});
    OK(stary.trasa(0).matcher.typ() == AsyncURIMatcher::BACKWARD);
    OK(stary.dopasuj("/history", HTTP_GET) == 0);
    OK(stary.dopasuj("/history/long", HTTP_GET) == 0);   // BLAD: powinno byc 1
    OK(stary.dopasuj("/history/link", HTTP_GET) == 0);   // BLAD: powinno byc 2
    // ... i dlatego po poprawce musi byc inaczej:
    OK(wifiWebServer.dopasuj("/history/long", HTTP_GET) == 1);
    OK(wifiWebServer.dopasuj("/history/link", HTTP_GET) == 2);
  }

  // ---- 03: adresy z PANELU (plik z panel218.mjs) -----------------------
  std::vector<std::vector<std::string>> sekwencje;
  if (argc > 1) {
    for (const std::string &l : wczytajTsv(argv[1], 3)) sekwencje.push_back(podziel(l));
  }
  grupa("panel podaje adresy, ktore serwer przyjmuje (z panel218.mjs)");
  {
    sprawdz(sekwencje.size() == 2, "panel dostarczyl 2 sekwencje (krotka i dluga)", __LINE__);
    if (sekwencje.size() == 2) {
      for (const auto &s : sekwencje) {
        const Adres218 biletAdres = rozbierz218(s[1]);
        const Adres218 pobranieAdres = rozbierz218(s[2]);
        sprawdz(wifiWebServer.dopasuj(String(biletAdres.sciezka.c_str()), HTTP_GET) == 2,
                "adres biletu z panelu trafia do trasy /history/link", __LINE__);
        const int oczekiwana = (s[0] == "dluga") ? 1 : 0;
        sprawdz(wifiWebServer.dopasuj(String(pobranieAdres.sciezka.c_str()), HTTP_GET) == oczekiwana,
                "adres pobrania z panelu trafia do wlasciwej trasy", __LINE__);
        sprawdz(pobranieAdres.parametry.size() == 1, "adres pobrania ma dokladnie jeden parametr", __LINE__);
        if (!pobranieAdres.parametry.empty()) {
          sprawdz(pobranieAdres.parametry[0].first == "k", "parametr pobrania to k", __LINE__);
          sprawdz(pobranieAdres.parametry[0].second.size() == historia213::kZnakow,
                  "bilet w adresie ma 32 znaki", __LINE__);
        }
      }
    }
  }

  // ---- 04: krotka historia, koniec do konca ----------------------------
  grupa("krotka: bilet -> 200, CSV == plik, bilet jednorazowy");
  {
    przygotuj();
    const std::string tresc = zrobCsv("krotka", 40, 20);
    g_ram218.utworz(historia211::kShortVfsPath, tresc);
    g_histShortSize = tresc.size();
    const std::string biletUrl = sekwencje.size() == 2 ? sekwencje[0][1] : "/history/link?stream=short";

    Wynik218 b = wyslij218(biletUrl);
    OK(b.trasa == 2);
    OK(b.kod == 200);
    OK(wszystkieHex218(b.cialo));
    OK(maNaglowek218(b.odpowiedz, "Cache-Control", "no-store"));
    const std::string bilet = b.cialo;
    delete b.odpowiedz;

    const std::string pobranie = (sekwencje.size() == 2 ? rozbierz218(sekwencje[0][2]).sciezka
                                                        : std::string("/history")) +
                                 "?k=" + bilet;
    Wynik218 w = wyslij218(pobranie);
    OK(w.trasa == 0);
    OK(w.kod == 200);
    OK(w.odpowiedz != nullptr);
    if (w.odpowiedz != nullptr) {
      OK(w.odpowiedz->_contentType == "text/csv");
      OK(maNaglowek218(w.odpowiedz, "Content-Disposition", "attachment; filename=\"historia-krotka.csv\""));
      Odebrane218 o = odbierz218(w.odpowiedz);
      OK(o.cialo == tresc);
      OK(o.cialo.size() > 101);          // wiecej niz jedna porcja bufora
      OK(o.zrodloWazne);
    }
    delete w.odpowiedz;

    // drugi raz ten sam bilet - musi byc 401 (bilet jednorazowy)
    Wynik218 w2 = wyslij218(pobranie);
    OK(w2.trasa == 0);
    OK(w2.kod == 401);
    delete w2.odpowiedz;
  }

  // ---- 05: dluga historia, koniec do konca -----------------------------
  grupa("dluga: adres z panelu trafia na /history/long, CSV == plik dlugi");
  {
    przygotuj();
    const std::string dluga = zrobCsv("dluga", 30, 34);
    g_ram218.utworz(historia211::kLongVfsPath, dluga);
    g_histLongSize = dluga.size();
    const std::string biletUrl = sekwencje.size() == 2 ? sekwencje[1][1] : "/history/link?stream=long";

    Wynik218 b = wyslij218(biletUrl);
    OK(b.trasa == 2);
    OK(b.kod == 200);
    OK(wszystkieHex218(b.cialo));
    const std::string bilet = b.cialo;
    delete b.odpowiedz;

    const std::string pobranie =
        (sekwencje.size() == 2 ? rozbierz218(sekwencje[1][2]).sciezka : std::string("/history/long")) +
        "?k=" + bilet;
    Wynik218 w = wyslij218(pobranie);
    OK(w.trasa == 1);            // KLUCZ: nie trasa 0
    OK(w.kod == 200);
    if (w.odpowiedz != nullptr) {
      OK(maNaglowek218(w.odpowiedz, "Content-Disposition", "attachment; filename=\"historia-dluga.csv\""));
      Odebrane218 o = odbierz218(w.odpowiedz);
      OK(o.cialo == dluga);
    }
    delete w.odpowiedz;
  }

  // ---- 06: bilet zwiazany ze strumieniem --------------------------------
  grupa("bilet dlugi na trasie krotkiej (i odwrotnie) to 401");
  {
    przygotuj();
    const std::string tresc = zrobCsv("krotka", 5, 10);
    g_ram218.utworz(historia211::kShortVfsPath, tresc);
    g_histShortSize = tresc.size();
    g_ram218.utworz(historia211::kLongVfsPath, tresc);
    g_histLongSize = tresc.size();

    Wynik218 b = wyslij218("/history/link?stream=long");
    OK(b.kod == 200);
    const std::string biletDlugi = b.cialo;
    delete b.odpowiedz;
    Wynik218 zle = wyslij218("/history?k=" + biletDlugi);
    OK(zle.trasa == 0);
    OK(zle.kod == 401);
    delete zle.odpowiedz;

    Wynik218 b2 = wyslij218("/history/link?stream=short");
    OK(b2.kod == 200);
    const std::string biletKrotki = b2.cialo;
    delete b2.odpowiedz;
    Wynik218 zle2 = wyslij218("/history/long?k=" + biletKrotki);
    OK(zle2.trasa == 1);
    OK(zle2.kod == 401);
    delete zle2.odpowiedz;
  }

  // ---- 07: bez poswiadczen i bez biletu --------------------------------
  grupa("bez biletu: /history i /history/long 401, /history/link 200");
  {
    przygotuj();
    const std::string tresc = zrobCsv("krotka", 3, 8);
    g_ram218.utworz(historia211::kShortVfsPath, tresc);
    g_histShortSize = tresc.size();

    Wynik218 a = wyslij218("/history");
    OK(a.trasa == 0);
    OK(a.kod == 401);
    delete a.odpowiedz;

    Wynik218 b = wyslij218("/history/long");
    OK(b.trasa == 1);
    OK(b.kod == 401);
    delete b.odpowiedz;

    // To jest regresja na najwazniejszy objaw bledu: ZADANIE O BILET nie moze
    // trafic do trasy pobrania.
    Wynik218 l = wyslij218("/history/link?stream=short");
    OK(l.trasa == 2);
    OK(l.kod == 200);
    OK(wszystkieHex218(l.cialo));
    delete l.odpowiedz;
  }

  // ---- 08: metoda inna niz GET -----------------------------------------
  grupa("metoda: POST /history -> 405, POST /history/link nie istnieje");
  {
    przygotuj();
    Wynik218 a = wyslij218("/history", HTTP_POST);
    OK(a.trasa == 0);
    OK(a.kod == 405);
    OK(maNaglowek218(a.odpowiedz, "Allow", "GET"));
    delete a.odpowiedz;

    Wynik218 b = wyslij218("/history/link", HTTP_POST);
    OK(b.trasa == -1);   // trasa jest tylko na GET - nie ma czego obslugiwac
  }

  // ---- 09: bilet wygasa ------------------------------------------------
  grupa("bilet wygasa po 5 minutach");
  {
    przygotuj();
    const std::string tresc = zrobCsv("krotka", 4, 6);
    g_ram218.utworz(historia211::kShortVfsPath, tresc);
    g_histShortSize = tresc.size();
    Wynik218 b = wyslij218("/history/link?stream=short");
    OK(b.kod == 200);
    const std::string bilet = b.cialo;
    delete b.odpowiedz;
    g_zegar211 += 6UL * 60UL * 1000UL;
    Wynik218 w = wyslij218("/history?k=" + bilet);
    OK(w.kod == 401);
    delete w.odpowiedz;
    // Drugia proba przy tym samym zegarze - bilet nadal wygasly.
    // (Na urzadzeniu millis() nie cofa sie, wiec to jest jedyny uczciwy wariant.)
    Wynik218 w2 = wyslij218("/history?k=" + bilet);
    OK(w2.kod == 401);
    delete w2.odpowiedz;
    g_zegar211 -= 6UL * 60UL * 1000UL;
  }

  // ---- 10: naglowek Bearer nadal dziala (regresja etapu 212) -----------
  grupa("stara droga: naglowek Bearer dziala bez zmian");
  {
    przygotuj();
    const std::string tresc = zrobCsv("krotka", 6, 12);
    g_ram218.utworz(historia211::kShortVfsPath, tresc);
    g_histShortSize = tresc.size();
    Wynik218 w = wyslij218("/history", HTTP_GET, "Bearer token-testowy-0123456789");
    OK(w.trasa == 0);
    OK(w.kod == 200);
    if (w.odpowiedz != nullptr) {
      Odebrane218 o = odbierz218(w.odpowiedz);
      OK(o.cialo == tresc);
    }
    delete w.odpowiedz;

    Wynik218 zly = wyslij218("/history", HTTP_GET, "Bearer nie-ten-token");
    OK(zly.kod == 401);
    delete zly.odpowiedz;
  }

  std::printf("%s sprawdzen=%d grup=%d\n", g_bledow == 0 ? "PASS trasa218" : "FAIL trasa218",
              g_sprawdzen, g_grupa);
  return g_bledow == 0 ? 0 : 1;
}
