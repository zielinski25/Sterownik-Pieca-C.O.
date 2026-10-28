// 213: test hostowy PRAWDZIWEJ trasy pobrania (v3.56.1) i PRAWDZIWEGO panelu.
//
// Co jest prawdziwe: naglowek HistoriaPobranie.h (212 + obsluga biletow),
// HistoriaBilety.h, bramka i obiekt trasy wyciagniety z main_centrala.cpp po
// patchu v3.56.0 (Extracted212.inc), adaptery losowosci/zegara, rejestracje tras
// i tekst panelu wyciagniete z pliku po patchu v3.56.1 (Extracted213.inc).
// Modelowane jest tylko to, czego firmware nie ma na komputerze: serwer HTTP,
// VFS w RAM, semafory i esp_random.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "../wysylka-zapinka211/host/Arduino.h"
#include "../wysylka-zapinka211/host/RamCalls.h"
#include "HistoriaPin.h"
#include "Serwer213.h"
#include "HistoriaPobranie.h"
#include "Model213.h"

// --- globalne obiekty centrali, ktorych dotyka bramka --------------------
bool littlefsReady = true;
volatile size_t g_histShortSize = 0;
volatile size_t g_histLongSize = 0;
SemaphoreHandle_t logMutex = nullptr;
RamCalls g_ram213;
history193::Calls &historiaSys = g_ram213;
bool g_historiaGotowa213 = true;
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

bool historiaDostepGotowy() { return g_historiaGotowa213; }

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

// --- PRAWDZIWY kod z etapu 212: bramka + obiekt trasy --------------------
#include "../pobranie-trasa212/Extracted212.inc"

// --- PRAWDZIWY kod z etapu 213: adaptery, rejestracje, tekst panelu ------
#include "Extracted213.inc"

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

// Bilety z PRAWDZIWYMI adapterami z firmware (historiaLosuj213/historiaZegar213).
historia213::Bilety bilety(historiaLosuj213, historiaZegar213);

void przygotuj() {
  g_ram213.wyczysc();
  g_historiaGotowa213 = true;
  littlefsReady = true;
  g_histShortSize = 0;
  g_histLongSize = 0;
  g_blokadaZajeta211 = false;
  g_zegar211 = 1000;
  g_los213 = 1;
  bilety.wyczysc();
}

std::string powtorz(char c, size_t n) { return std::string(n, c); }

bool wszystkieHex(const std::string &s) {
  if (s.size() != historia213::kZnakow) return false;
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

struct Wynik {
  int kod = 0;
  AsyncWebServerResponse *odpowiedz = nullptr;
  std::string cialo;
};

// Wywolanie trasy: parametry zapytania + opcjonalny naglowek Authorization.
Wynik zadanie(historia212::Pobranie &p, historia212::Strumien s, int metoda = HTTP_GET,
              const char *auth = "Bearer token-testowy-0123456789",
              const std::vector<std::pair<const char *, const char *>> &parametry = {}) {
  AsyncWebServerRequest r;
  r.metoda = metoda;
  if (auth != nullptr) r.dodajNaglowek("Authorization", auth);
  for (const auto &kv : parametry) r.dodajParametr(kv.first, kv.second);
  p.obsluz(&r, s);
  Wynik w;
  w.kod = r.kod;
  w.odpowiedz = r.wyslana;
  w.cialo = r.beginCialo;   // model zapisuje cialo beginResponse - bez RTTI
  return w;
}

// Wywolanie PRAWDZIWEJ rejestracji /history/link (ta sama funkcja, ktora
// firmware podpina do serwera), na modelu zadania.
Wynik link(const char *stream, int metoda = HTTP_GET) {
  AsyncWebServerRequest r;
  r.metoda = metoda;
  if (stream != nullptr) r.dodajParametr("stream", stream);
  obslugaHistoryLink(&r);
  Wynik w;
  w.kod = r.kod;
  w.odpowiedz = r.wyslana;
  w.cialo = r.beginCialo;
  return w;
}

std::string pobierzBilet(const char *stream) {
  Wynik w = link(stream);
  sprawdz(w.kod == 200, "bilet wydany (200)", __LINE__);
  const std::string bilet = w.cialo;
  delete w.odpowiedz;   // model nie ma wlasciciela odpowiedzi - sprzatamy sami
  return bilet;
}

#define ODPOWIEDZ_ALBO_STOP(o, w)                                                       \
  AsyncAbstractResponse *o = (w).odpowiedz ? (w).odpowiedz->jakoStrumien() : nullptr;   \
  sprawdz(o != nullptr, "odpowiedz 200 jest AsyncAbstractResponse", __LINE__);          \
  if (o == nullptr) {                                                                   \
    delete (w).odpowiedz;                                                               \
    return 1;                                                                           \
  }

bool maNaglowek(AsyncWebServerResponse *r, const char *n, const char *v) {
  if (!r) return false;
  for (const auto &h : r->naglowki) {
    if (h.first == n && h.second == v) return true;
  }
  return false;
}

struct Wysylka213 {
  std::string cialo;
  bool zrodloWazne = false;
};

Wysylka213 odbierz(AsyncAbstractResponse *r, std::size_t bufor = 101) {
  Wysylka213 w;
  std::vector<uint8_t> b(bufor);
  for (int i = 0; i < 1000000; ++i) {
    const std::size_t n = r->_fillBuffer(b.data(), b.size());
    if (n == 0) break;
    w.cialo.append(reinterpret_cast<const char *>(b.data()), n);
  }
  w.zrodloWazne = r->_sourceValid();
  return w;
}
}  // namespace

int main() {
  logMutex = xSemaphoreCreateMutex();
  historiaPobranie.ustawBilety(&bilety);

  // ---- 01: bilet + pobranie krotkie -----------------------------------
  grupa("01 bilet 32 hex, pobranie krotkie 200, bilet jednorazowy");
  {
    przygotuj();
    const std::string tresc = "a,x\nb,y\nc,z\n";
    g_ram213.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    // jeden bilet, sprawdzamy przy okazji naglowek Cache-Control
    Wynik l = link("short");
    OK(l.kod == 200);
    OK(maNaglowek(l.odpowiedz, "Cache-Control", "no-store"));  // bilet nie moze sie cachowac
    const std::string b = l.cialo;
    delete l.odpowiedz;
    OK(wszystkieHex(b));
    OK(bilety.zajete() == 1);
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                      {{"k", b.c_str()}});
    OK(w.kod == 200);
    ODPOWIEDZ_ALBO_STOP(o, w)
    OK(o->_contentLength == tresc.size());
    Wysylka213 wys = odbierz(o);
    OK(wys.cialo == tresc);
    delete w.odpowiedz;
    OK(bilety.zajete() == 0);
    // drugi raz ten sam bilet
    Wynik w2 = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                       {{"k", b.c_str()}});
    OK(w2.kod == 401);
    delete w2.odpowiedz;
  }

  // ---- 02: bilet na dluga nie otwiera krotkiej ------------------------
  grupa("02 bilet zwiazany ze strumieniem: dluga != krotka");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_long.csv", "1,2\n3,4\n");
    g_histLongSize = 8;
    const std::string b = pobierzBilet("long");
    Wynik zle = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                        {{"k", b.c_str()}});
    OK(zle.kod == 401);
    OK(bilety.zajete() == 1);  // zly strumien NIE spala biletu
    delete zle.odpowiedz;
    Wynik dobre = zadanie(historiaPobranie, historia212::Strumien::Dluga, HTTP_GET, nullptr,
                          {{"k", b.c_str()}});
    OK(dobre.kod == 200);
    AsyncAbstractResponse *o = dobre.odpowiedz ? dobre.odpowiedz->jakoStrumien() : nullptr;
    OK(o != nullptr);
    if (o != nullptr) {
      Wysylka213 wys = odbierz(o);
      OK(wys.cialo == "1,2\n3,4\n");
    }
    delete dobre.odpowiedz;
  }

  // ---- 03: brak poswiadczen -------------------------------------------
  grupa("03 brak naglowka i brak biletu: 401 + WWW-Authenticate");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr);
    OK(w.kod == 401);
    OK(maNaglowek(w.odpowiedz, "WWW-Authenticate", "Bearer"));
    OK(g_ram213.otwarciem == 0);
    delete w.odpowiedz;
  }

  // ---- 04: zla dlugosc biletu -----------------------------------------
  grupa("04 bilet o zlej dlugosci: 401, bez otwierania pliku");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    for (const char *zly : {"", "0123456789abcdef0123456789abcde",
                            "0123456789abcdef0123456789abcdef0"}) {
      Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                        {{"k", zly}});
      OK(w.kod == 401);
      delete w.odpowiedz;
    }
    // bilet PRAWIDLOWY, ale z doklejonym znakiem albo skrocony - tez nie dziala
    const std::string dobry = pobierzBilet("short");
    const std::string dluzszy = dobry + "0";
    const std::string krotszy = dobry.substr(0, historia213::kZnakow - 1);
    for (const std::string &wariant : {dluzszy, krotszy}) {
      Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                        {{"k", wariant.c_str()}});
      OK(w.kod == 401);
      delete w.odpowiedz;
    }
    OK(g_ram213.otwarciem == 0);
  }

  // ---- 05: nieznany bilet ---------------------------------------------
  grupa("05 nieznany bilet 32 hex: 401");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                      {{"k", "0123456789abcdef0123456789abcdef"}});
    OK(w.kod == 401);
    delete w.odpowiedz;
  }

  // ---- 06: waznosc 5 minut --------------------------------------------
  grupa("06 bilet wazny 299999 ms, po 300000 ms juz nie");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    const std::string b = pobierzBilet("short");
    zegar213(1000 + historia213::kWaznoscMs - 1);
    Wynik ok = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                       {{"k", b.c_str()}});
    OK(ok.kod == 200);
    delete ok.odpowiedz;
    const std::string b2 = pobierzBilet("short");
    zegar213(1000 + 2 * historia213::kWaznoscMs);
    Wynik zle = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                        {{"k", b2.c_str()}});
    OK(zle.kod == 401);
    delete zle.odpowiedz;
  }

  // ---- 07: przewiniecie millis() --------------------------------------
  grupa("07 przewiniecie millis() po ~49 dniach nie psuje waznosci");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    zegar213(0xFFFFFF00u);
    const std::string b = pobierzBilet("short");
    zegar213(0x00000000u + 60u * 1000u);  // 1 minuta po przewinieciu
    Wynik ok = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                       {{"k", b.c_str()}});
    OK(ok.kod == 200);
    delete ok.odpowiedz;
    zegar213(0xFFFFFF00u + 1u);
    const std::string b2 = pobierzBilet("short");
    zegar213(0xFFFFFF00u + 6u * 60u * 1000u);  // 6 minut pozniej, po przewinieciu
    Wynik zle = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                        {{"k", b2.c_str()}});
    OK(zle.kod == 401);
    delete zle.odpowiedz;
  }

  // ---- 08: Bearer dziala i ma pierwszenstwo ---------------------------
  grupa("08 Bearer dziala; zly Bearer + dobry bilet = 401 (bez awaryjnego linku)");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    const std::string b = pobierzBilet("short");
    Wynik bearer = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(bearer.kod == 200);
    delete bearer.odpowiedz;
    Wynik zlyBearer = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET,
                             "Bearer zly-token", {{"k", b.c_str()}});
    OK(zlyBearer.kod == 401);
    delete zlyBearer.odpowiedz;
    OK(bilety.zajete() == 1);  // bilet nietkniety
    Wynik bilet = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                          {{"k", b.c_str()}});
    OK(bilet.kod == 200);
    delete bilet.odpowiedz;
  }

  // ---- 09: token nie ustawiony ----------------------------------------
  grupa("09 pusty token w secrets.h: link i pobranie 503");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    historia212::Pobranie bezTokenu(historiaBramka, "");
    bezTokenu.ustawBilety(&bilety);
    Wynik l = link("short");
    OK(l.kod == 200);  // stary obiekt ma token, wiec bilet dostaje
    delete l.odpowiedz;
    // a teraz obiekt bez tokenu
    AsyncWebServerRequest r;
    r.metoda = HTTP_GET;
    r.dodajParametr("stream", "short");
    bezTokenu.link(&r);
    OK(r.kod == 503);
    delete r.wyslana;
    AsyncWebServerRequest r2;
    r2.metoda = HTTP_GET;
    bezTokenu.obsluz(&r2, historia212::Strumien::Krotki);
    OK(r2.kod == 503);
    delete r2.wyslana;
  }

  // ---- 10: metoda na /history/link ------------------------------------
  grupa("10 POST na /history/link: 405 + Allow: GET");
  {
    przygotuj();
    for (int m : {HTTP_POST, HTTP_PUT, HTTP_DELETE}) {
      Wynik w = link("short", m);
      OK(w.kod == 405);
      OK(maNaglowek(w.odpowiedz, "Allow", "GET"));
      OK(w.cialo.empty() || w.kod == 405);
      delete w.odpowiedz;
    }
  }

  // ---- 11: zajeta trasa nie spala biletu ------------------------------
  grupa("11 trwa inne pobranie: 503, bilet NIE spalony");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    const std::string b = pobierzBilet("short");
    AsyncWebServerRequest pierwsze;
    pierwsze.metoda = HTTP_GET;
    pierwsze.dodajNaglowek("Authorization", "Bearer token-testowy-0123456789");
    historiaPobranie.obsluz(&pierwsze, historia212::Strumien::Krotki);
    OK(pierwsze.kod == 200);
    Wynik drugie = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                           {{"k", b.c_str()}});
    OK(drugie.kod == 503);
    delete drugie.odpowiedz;
    OK(bilety.zajete() == 1);
    delete pierwsze.wyslana;  // koniec pobrania
    Wynik po = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                       {{"k", b.c_str()}});
    OK(po.kod == 200);
    delete po.odpowiedz;
  }

  // ---- 12: magazyn niegotowy nie spala biletu -------------------------
  grupa("12 magazyn niegotowy: 503, bilet NIE spalony");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    const std::string b = pobierzBilet("short");
    g_historiaGotowa213 = false;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                      {{"k", b.c_str()}});
    OK(w.kod == 503);
    delete w.odpowiedz;
    g_historiaGotowa213 = true;
    Wynik po = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                       {{"k", b.c_str()}});
    OK(po.kod == 200);
    delete po.odpowiedz;
  }

  // ---- 13: brak pliku spala bilet (udokumentowane) --------------------
  grupa("13 brak pliku: 404, bilet spalony - drugi raz 401");
  {
    przygotuj();
    g_histShortSize = 0;
    const std::string b = pobierzBilet("short");
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                      {{"k", b.c_str()}});
    OK(w.kod == 404);
    delete w.odpowiedz;
    Wynik w2 = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                       {{"k", b.c_str()}});
    OK(w2.kod == 401);
    delete w2.odpowiedz;
  }

  // ---- 14: cztery miejsca, piaty bilet wypycha najstarszy -------------
  grupa("14 cztery bilety; piaty wypycha najstarszy");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    std::vector<std::string> b;
    for (int i = 0; i < 4; ++i) {
      zegar213(1000 + static_cast<std::uint32_t>(i));
      b.push_back(pobierzBilet("short"));
    }
    OK(bilety.zajete() == 4);
    const std::string piaty = pobierzBilet("short");
    OK(bilety.zajete() == 4);
    Wynik stary = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                          {{"k", b[0].c_str()}});
    OK(stary.kod == 401);
    delete stary.odpowiedz;
    Wynik nowy = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                         {{"k", piaty.c_str()}});
    OK(nowy.kod == 200);
    delete nowy.odpowiedz;
  }

  // ---- 15: bilety bez zrodel ------------------------------------------
  grupa("15 bilety bez zrodla losowosci albo zegara nie wydaja biletu");
  {
    char buf[historia213::kZnakow + 1];
    historia213::Bilety bezLosu(nullptr, historiaZegar213);
    OK(!bezLosu.wydaj(historia213::kKrotki, buf));
    historia213::Bilety bezZegara(historiaLosuj213, nullptr);
    OK(!bezZegara.wydaj(historia213::kKrotki, buf));
    historia213::Bilety dobre(historiaLosuj213, historiaZegar213);
    OK(dobre.wydaj(historia213::kKrotki, buf));
    OK(!dobre.zuzyj(nullptr, historia213::kZnakow, historia213::kKrotki));
  }

  // ---- 16: wyczysc() uniewaznia wszystko ------------------------------
  grupa("16 wyczysc() uniewaznia wszystkie bilety");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    const std::string b = pobierzBilet("short");
    bilety.wyczysc();
    OK(bilety.zajete() == 0);
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                      {{"k", b.c_str()}});
    OK(w.kod == 401);
    delete w.odpowiedz;
  }

  // ---- 17: tekst panelu -----------------------------------------------
  grupa("17 panel: siega po bilet, nie zna stalego tokenu");
  {
    const std::string p = PANEL_213;
    OK(p.find("/history/link?stream=") != std::string::npos);
    OK(p.find("pobierz('short')") != std::string::npos);
    OK(p.find("pobierz('long')") != std::string::npos);
    OK(p.find("'/history?k=' + encodeURIComponent(bilet)") != std::string::npos);
    OK(p.find("length !== 32") != std::string::npos);
    OK(p.find("id=\"histBtn\"") != std::string::npos);
    OK(p.find("id=\"histShort\"") != std::string::npos);
    OK(p.find("id=\"histLong\"") != std::string::npos);
    OK(p.find("HISTORIA_TOKEN") == std::string::npos);
    OK(p.find("SECRET_HISTORIA_TOKEN") == std::string::npos);
    OK(p.find("Bearer ") == std::string::npos);
    OK(p.find("textContent") != std::string::npos);
  }

  // ---- 18: rejestracje wyciagniete z firmware -------------------------
  grupa("18 /history, /history/long, /history/link: wlasciwe strumienie");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "krotka\n");
    g_ram213.utworz("/littlefs/hist_long.csv", "dluga-linia\n");
    g_histShortSize = 7;
    g_histLongSize = 12;

    AsyncWebServerRequest r1;
    r1.metoda = HTTP_GET;
    r1.dodajNaglowek("Authorization", "Bearer token-testowy-0123456789");
    obslugaHistory(&r1);
    OK(r1.kod == 200);
    AsyncAbstractResponse *o1 = r1.wyslana ? r1.wyslana->jakoStrumien() : nullptr;
    OK(o1 != nullptr);
    if (o1 != nullptr) {
      Wysylka213 w1 = odbierz(o1);
      OK(w1.cialo == "krotka\n");
    }
    delete r1.wyslana;

    AsyncWebServerRequest r2;
    r2.metoda = HTTP_GET;
    r2.dodajNaglowek("Authorization", "Bearer token-testowy-0123456789");
    obslugaHistoryLong(&r2);
    OK(r2.kod == 200);
    AsyncAbstractResponse *o2 = r2.wyslana ? r2.wyslana->jakoStrumien() : nullptr;
    OK(o2 != nullptr);
    if (o2 != nullptr) {
      Wysylka213 w2 = odbierz(o2);
      OK(w2.cialo == "dluga-linia\n");
    }
    delete r2.wyslana;

    AsyncWebServerRequest r3;
    r3.metoda = HTTP_GET;
    r3.dodajParametr("stream", "long");
    obslugaHistoryLink(&r3);
    OK(r3.kod == 200);
    OK(wszystkieHex(r3.beginCialo));
    delete r3.wyslana;
  }

  // ---- 19: 20 pobran pod rzad -----------------------------------------
  grupa("19 20 pobran pod rzad: brak wycieku biletow i uchwytow");
  {
    przygotuj();
    const std::string tresc = powtorz('q', 512);
    g_ram213.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    int udane = 0;
    for (int i = 0; i < 20; ++i) {
      const std::string b = pobierzBilet("short");
      Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr,
                        {{"k", b.c_str()}});
      if (w.kod == 200) {
        AsyncAbstractResponse *o = w.odpowiedz->jakoStrumien();
        Wysylka213 wys = odbierz(o);
        if (wys.cialo == tresc) ++udane;
      }
      delete w.odpowiedz;
    }
    OK(udane == 20);
    OK(bilety.zajete() == 0);
    OK(g_ram213.otwartych == 0);   // zaden uchwyt nie zostal otwarty
    OK(g_ram213.otwarciem == 20);  // dokladnie jedno otwarcie na pobranie
  }

  // ---- 20: bilet nie dotyka pliku -------------------------------------
  grupa("20 wydanie biletu nie otwiera pliku historii");
  {
    przygotuj();
    g_ram213.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    (void)pobierzBilet("short");
    OK(g_ram213.otwarciem == 0);
    OK(g_ram213.otwartych == 0);
  }

  if (g_bledow == 0) {
    std::printf("\nPASS panel213 grup=%d sprawdzen=%d bledow=0\n", g_grupa, g_sprawdzen);
  } else {
    std::printf("\nFAIL panel213 grup=%d sprawdzen=%d bledow=%d\n", g_grupa, g_sprawdzen, g_bledow);
  }
  return g_bledow == 0 ? 0 : 1;
}
