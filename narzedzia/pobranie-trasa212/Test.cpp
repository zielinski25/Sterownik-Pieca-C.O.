// 212: test hostowy PRAWDZIWEJ trasy pobrania historii i PRAWDZIWEJ bramki
// (wyciagnietych z main_centrala.cpp po patchu v3.56.0). Zapinka, VFS i modele
// pochodza z etapu 211; modelowany jest tylko interfejs serwera HTTP.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../wysylka-zapinka211/host/Arduino.h"
#include "../wysylka-zapinka211/host/RamCalls.h"
#include "HistoriaPin.h"
#include "Serwer212.h"
#include "HistoriaPobranie.h"

// --- globalne obiekty centrali, ktorych dotyka bramka --------------------
bool littlefsReady = true;
volatile size_t g_histShortSize = 0;
volatile size_t g_histLongSize = 0;
SemaphoreHandle_t logMutex = nullptr;
RamCalls g_ram212;
history193::Calls &historiaSys = g_ram212;
bool g_historiaGotowa212 = true;
const char *HISTORIA_TOKEN = "token-testowy-0123456789";
unsigned long g_zegar211 = 0;
int g_psmallocFail211 = 0;
int g_mallocFail211 = 0;
int g_diagErr211 = 0;
int g_diagWarn211 = 0;
bool g_blokadaZajeta211 = false;
WiFiClass WiFi;
SerialClass Serial;

bool historiaDostepGotowy() { return g_historiaGotowa212; }

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

// --- PRAWDZIWY kod po patchu: bramka + obiekty trasy ---------------------
#include "Extracted212.inc"

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

void przygotuj() {
  g_ram212.wyczysc();
  g_historiaGotowa212 = true;
  littlefsReady = true;
  g_histShortSize = 0;
  g_histLongSize = 0;
  g_blokadaZajeta211 = false;
  g_zegar211 = 0;
}

std::string powtorz(char c, size_t n) { return std::string(n, c); }

// Wywolanie trasy na modelu zadania. Zwraca kod HTTP i (jesli 200) gotowa
// odpowiedz do odebrania.
struct Wynik {
  int kod = 0;
  AsyncWebServerResponse *odpowiedz = nullptr;
};

Wynik zadanie(historia212::Pobranie &p, historia212::Strumien s, int metoda = HTTP_GET,
              const char *auth = "Bearer token-testowy-0123456789") {
  AsyncWebServerRequest r;
  r.metoda = metoda;
  if (auth) r.dodajNaglowek("Authorization", auth);
  p.obsluz(&r, s);
  Wynik w;
  w.kod = r.kod;
  w.odpowiedz = r.wyslana;
  return w;
}

// Rzutowanie na odpowiedz strumieniowa. Jesli trasa zwrocila odmowe, test
// NIE moze sie wywrocic na nullptr - ma pokazac blad i skonczyc.
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
}  // namespace

int main() {
  logMutex = xSemaphoreCreateMutex();
  // historiaPobranie i historiaBramka pochodza z Extracted212.inc (prawdziwy plik).

  // ---- 01: poprawne pobranie ------------------------------------------
  grupa("01 GET + Bearer: 200, dlugosc i cialo z pliku");
  {
    przygotuj();
    const std::string tresc = "a,x\nb,y\nc,z\n";
    g_ram212.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 200);
    ODPOWIEDZ_ALBO_STOP(o, w)
    OK(o->_contentLength == tresc.size());
    OK(o->_sendContentLength);
    OK(!o->_chunked);
    Wysylka wys = wyslij(o);
    OK(wys.cialo == tresc);
    OK(wys.zrodloByloWazneNaKoncu);
    OK(historiaPobranie.zajeta());
    delete w.odpowiedz;
    OK(!historiaPobranie.zajeta());
    OK(g_ram212.otwartych == 0);
  }

  // ---- 02: zla metoda --------------------------------------------------
  grupa("02 POST/PUT/DELETE/HEAD: 405 + Allow, plik nietkniety");
  {
    for (int m : {HTTP_POST, HTTP_PUT, HTTP_DELETE}) {
      przygotuj();
      g_ram212.utworz("/littlefs/hist_short.csv", "abc\n");
      g_histShortSize = 4;
      Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, m);
      OK(w.kod == 405);
      OK(maNaglowek(w.odpowiedz, "Allow", "GET"));
      OK(g_ram212.otwarciem == 0);
      OK(!historiaPobranie.zajeta());
      delete w.odpowiedz;
    }
  }

  // ---- 03: brak tokenu w naglowku --------------------------------------
  grupa("03 brak Authorization: 401 + WWW-Authenticate");
  {
    przygotuj();
    g_ram212.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, nullptr);
    OK(w.kod == 401);
    OK(maNaglowek(w.odpowiedz, "WWW-Authenticate", "Bearer"));
    OK(g_ram212.otwarciem == 0);
    delete w.odpowiedz;
  }

  // ---- 04: zle poswiadczenia ------------------------------------------
  grupa("04 zly token, Basic, goły token, inna wielkosc liter: 401");
  {
    const char *zle[] = {
        "Bearer token-testowy-0123456788",
        "Bearer token-testowy-012345678",
        "Bearer token-testowy-01234567890",
        "Basic token-testowy-0123456789",
        "token-testowy-0123456789",
        "Bearer ",
        "Bearerx token-testowy-0123456789",
        "Bearer Token-testowy-0123456789",
        // RFC 7235: po schemacie jest 1*SP, tabulacja nie jest dozwolona
        "Bearer\ttoken-testowy-0123456789",
    };
    for (const char *a : zle) {
      przygotuj();
      g_ram212.utworz("/littlefs/hist_short.csv", "abc\n");
      g_histShortSize = 4;
      Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, a);
      OK(w.kod == 401);
      OK(g_ram212.otwarciem == 0);
      delete w.odpowiedz;
    }
  }

  // ---- 05: schemat bez wzgledu na wielkosc liter, OWS wokol tokenu -----
  grupa("05 BEARER/bEaReR i spacje wokol tokenu: 200");
  {
    const char *dobre[] = {"Bearer token-testowy-0123456789",
                           "BEARER token-testowy-0123456789",
                           "bEaReR token-testowy-0123456789",
                           "Bearer   token-testowy-0123456789  "};
    for (const char *a : dobre) {
      przygotuj();
      const std::string tresc = "q\n";
      g_ram212.utworz("/littlefs/hist_short.csv", tresc);
      g_histShortSize = tresc.size();
      Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki, HTTP_GET, a);
      OK(w.kod == 200);
      delete w.odpowiedz;
    }
  }

  // ---- 06: token nieskonfigurowany ------------------------------------
  grupa("06 pusty token w konfiguracji: 503, pobieranie wylaczone");
  {
    przygotuj();
    g_ram212.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    historia212::Pobranie p(historiaBramka, "");
    AsyncWebServerRequest r;
    r.dodajNaglowek("Authorization", "Bearer cokolwiek");
    p.obsluz(&r, historia212::Strumien::Krotki);
    OK(r.kod == 503);
    OK(g_ram212.otwarciem == 0);
    delete r.wyslana;
    historia212::Pobranie p2(historiaBramka, nullptr);
    AsyncWebServerRequest r2;
    r2.dodajNaglowek("Authorization", "Bearer cokolwiek");
    p2.obsluz(&r2, historia212::Strumien::Krotki);
    OK(r2.kod == 503);
    delete r2.wyslana;
  }

  // ---- 07: blokada pisarza zajeta --------------------------------------
  grupa("07 logMutex zajety: 503, ani jednego otwarcia pliku");
  {
    przygotuj();
    const std::string tresc = "abc\n";
    g_ram212.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    g_blokadaZajeta211 = true;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 503);
    OK(g_ram212.otwarciem == 0);
    OK(g_ram212.tresc("/littlefs/hist_short.csv") == tresc);
    delete w.odpowiedz;
  }

  // ---- 08: magazyn niegotowy ------------------------------------------
  grupa("08 magazyn niegotowy / brak LittleFS: 503");
  {
    przygotuj();
    g_ram212.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    g_historiaGotowa212 = false;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 503);
    delete w.odpowiedz;
    przygotuj();
    g_ram212.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    littlefsReady = false;
    Wynik w2 = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w2.kod == 503);
    delete w2.odpowiedz;
  }

  // ---- 09: brak pliku, pusty plik --------------------------------------
  grupa("09 brak pliku i zero potwierdzone: 404");
  {
    przygotuj();
    g_histShortSize = 4;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 404);
    delete w.odpowiedz;
    przygotuj();
    g_ram212.utworz("/littlefs/hist_short.csv", "");
    g_histShortSize = 0;
    // Plik ISTNIEJE, ale ma 0 B. Zapinka z etapu 211 klasyfikuje to jako
    // Fault (nie NothingCommitted), wiec trasa odpowiada 500, a nie 404.
    // Zachowanie jest swiadome i opisane w README (znane ograniczenie).
    Wynik w2 = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w2.kod == 500);
    delete w2.odpowiedz;
    OK(g_ram212.otwartych == 0);
  }

  // ---- 10: za duzy plik ------------------------------------------------
  grupa("10 prefiks powyzej limitu: 413, zapinka zamknieta");
  {
    przygotuj();
    const size_t n = 50u * 1024u * 1024u + 1u;
    g_ram212.utworz("/littlefs/hist_short.csv", powtorz('L', n));
    g_histShortSize = n;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 413);
    OK(g_ram212.otwartych == 0);
    delete w.odpowiedz;
  }

  // ---- 11: blad VFS przy otwieraniu -----------------------------------
  grupa("11 VFS odmawia otwarcia: 500");
  {
    przygotuj();
    g_ram212.utworz("/littlefs/hist_short.csv", "abc\n");
    g_histShortSize = 4;
    g_ram212.odmowaOtwarciu = true;
    // Zapinka z 211 zwraca Missing dla KAZDEJ nieudanej open() (nie odroznia
    // ENOENT od EIO), wiec trasa odpowiada 404. Znane ograniczenie, README.
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 404);
    delete w.odpowiedz;
  }

  // ---- 12: drugie pobranie w trakcie ----------------------------------
  grupa("12 dwa pobrania naraz: drugie 503, bez otwierania pliku");
  {
    przygotuj();
    const std::string tresc = "abc\n";
    g_ram212.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    Wynik pierwsze = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(pierwsze.kod == 200);
    const int otwarciemPoPierwszym = g_ram212.otwarciem;
    Wynik drugie = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(drugie.kod == 503);
    OK(g_ram212.otwarciem == otwarciemPoPierwszym);
    delete drugie.odpowiedz;
    delete pierwsze.odpowiedz;
    OK(!historiaPobranie.zajeta());
    OK(g_ram212.otwartych == 0);
  }

  // ---- 13: historia dluga ---------------------------------------------
  grupa("13 /history/long: inny plik, inna dlugosc");
  {
    przygotuj();
    const std::string dluga = powtorz('D', 333);
    g_ram212.utworz("/littlefs/hist_long.csv", dluga);
    g_ram212.utworz("/littlefs/hist_short.csv", "krotka\n");
    g_histLongSize = dluga.size();
    g_histShortSize = 7;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Dluga);
    OK(w.kod == 200);
    ODPOWIEDZ_ALBO_STOP(o, w)
    OK(o->_contentLength == dluga.size());
    Wysylka wys = wyslij(o);
    OK(wys.cialo == dluga);
    delete w.odpowiedz;
    OK(g_ram212.otwartych == 0);
  }

  // ---- 14: znacznik zapisu wyzej niz plik ------------------------------
  grupa("14 znacznik wyzej niz plik: wysylamy tylko to, co jest");
  {
    przygotuj();
    const std::string tresc = powtorz('m', 40);
    g_ram212.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = 1000;
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 200);
    ODPOWIEDZ_ALBO_STOP(o, w)
    OK(o->_contentLength == 40);
    Wysylka wys = wyslij(o);
    OK(wys.cialo == tresc);
    OK(wys.zrodloByloWazneNaKoncu);
    delete w.odpowiedz;
  }

  // ---- 15: plik skrocony w trakcie wysylania ---------------------------
  grupa("15 plik skrocony w polowie: zrodlo NIEWAZNE, biblioteka zerwie");
  {
    przygotuj();
    const std::string tresc = powtorz('s', 400);
    g_ram212.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    OK(w.kod == 200);
    ODPOWIEDZ_ALBO_STOP(o, w)
    OK(o->_contentLength == 400);
    uint8_t bufor[100];
    const size_t n1 = o->_fillBuffer(bufor, sizeof bufor);
    OK(n1 == 100);
    OK(o->_sourceValid());
    g_ram212.przytnij("/littlefs/hist_short.csv", 120);
    size_t odebrane = n1;
    for (;;) {
      const size_t n = o->_fillBuffer(bufor, sizeof bufor);
      if (n == 0) break;
      odebrane += n;
      if (odebrane > 1000) break;
    }
    OK(odebrane < 400);          // nie udalo sie wydac calego zapowiedzianego prefiksu
    OK(!o->_sourceValid());      // biblioteka NIE zakonczy tego jako 200
    delete w.odpowiedz;
    OK(g_ram212.otwartych == 0);
  }

  // ---- 16: bufor 1 bajt ------------------------------------------------
  grupa("16 bufor odpowiedzi 1 bajt: cialo nadal dokladne");
  {
    przygotuj();
    const std::string tresc = powtorz('j', 257);
    g_ram212.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
    ODPOWIEDZ_ALBO_STOP(o, w)
    Wysylka wys = wyslij(o, 1);
    OK(wys.cialo == tresc);
    OK(wys.porcji == 257);
    delete w.odpowiedz;
  }

  // ---- 17: bramka - potwierdzony i sciezka -----------------------------
  grupa("17 bramka: wlasciwy znacznik i wlasciwa sciezka dla strumienia");
  {
    przygotuj();
    g_histShortSize = 111;
    g_histLongSize = 222;
    OK(historiaBramka.potwierdzony(historia212::Strumien::Krotki) == 111);
    OK(historiaBramka.potwierdzony(historia212::Strumien::Dluga) == 222);
    OK(std::strcmp(historiaBramka.sciezka(historia212::Strumien::Krotki),
                   historia211::kShortVfsPath) == 0);
    OK(std::strcmp(historiaBramka.sciezka(historia212::Strumien::Dluga),
                   historia211::kLongVfsPath) == 0);
  }

  // ---- 18: bramka - magazynGotowy --------------------------------------
  grupa("18 bramka: magazynGotowy wymaga OBU warunkow");
  {
    for (int a = 0; a < 2; ++a) {
      for (int b = 0; b < 2; ++b) {
        przygotuj();
        g_historiaGotowa212 = (a == 1);
        littlefsReady = (b == 1);
        OK(historiaBramka.magazynGotowy() == (a == 1 && b == 1));
      }
    }
  }

  // ---- 19: bramka - zamknij/otworz naprawde bierze logMutex -------------
  grupa("19 bramka: zamknij bierze logMutex, otworz oddaje");
  {
    przygotuj();
    OK(historiaBramka.zamknij());
    OK(xSemaphoreTake(logMutex, 0) == pdFALSE);  // ktos trzyma
    historiaBramka.otworz();
    OK(xSemaphoreTake(logMutex, 0) == pdTRUE);
    xSemaphoreGive(logMutex);
    g_blokadaZajeta211 = true;
    OK(!historiaBramka.zamknij());
    g_blokadaZajeta211 = false;
  }

  // ---- 20: 30 pobran pod rzad -----------------------------------------
  grupa("20 30 pobran pod rzad: brak wycieku zapinek i uchwytow");
  {
    przygotuj();
    const std::string tresc = powtorz('p', 123);
    g_ram212.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    int udane = 0;
    for (int i = 0; i < 30; ++i) {
      Wynik w = zadanie(historiaPobranie, historia212::Strumien::Krotki);
      if (w.kod != 200) continue;
      ++udane;
      ODPOWIEDZ_ALBO_STOP(o, w)
      Wysylka wys = wyslij(o);
      if (wys.cialo == tresc) ++udane;
      delete w.odpowiedz;
    }
    OK(udane == 60);
    OK(g_ram212.otwartych == 0);
    OK(!historiaPobranie.zajeta());
  }

  std::printf("\n%s grup=%d sprawdzen=%d bledow=%d\n",
              g_bledow == 0 ? "PASS trasa212" : "FAIL trasa212", g_grupa, g_sprawdzen, g_bledow);
  return g_bledow == 0 ? 0 : 1;
}
