// 217: WYTRZYMALOSC - setki pobran i setki wysylek na tym samym kodzie, ktory
// pojdzie na urzadzenie. Cel: wychwycic wycieki uchwytow plikow i "zapchana"
// flage pobrania, ktore na urzadzeniu objawiaja sie tak samo: "po kilkunastu
// pobraniach przestaje dzialac, pomaga restart".
//
// Co jest prawdziwe: HistoriaPobranie.h (trasa + odpowiedz), HistoriaBilety.h,
// HistoriaPin.h (zapinka), oba wysylacze dokumentow Telegram, bramka i lambdy
// tras - wszystko wyciagniete z pliku po patchu v3.56.3 (Extracted217.inc).
// Modelowany jest tylko swiat zewnetrzny: VFS w RAM, semafory, gniazdo TLS,
// zegar, alokacja (z mozliwoscia wstrzykniecia awarii).
//
// Wynik: "PASS wytrzymalosc217 sprawdzen=N bledow=0" albo FAIL i kod 1.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "Arduino.h"
#include "RamCalls.h"
#include "Firmware211.h"
#include "HistoriaPin.h"
#include "Serwer213.h"
#include "HistoriaPobranie.h"
#include "Model213.h"

// --- globalne obiekty "firmware'u" (modele) ------------------------------
unsigned long g_zegar211 = 0;
int g_psmallocFail211 = 0;
int g_mallocFail211 = 0;
int g_diagErr211 = 0;
int g_diagWarn211 = 0;
std::vector<std::string> g_diag217;
bool g_blokadaZajeta211 = false;
WiFiClass WiFi;
SerialClass Serial;

RamCalls g_ram217;
history193::Calls &historiaSys = g_ram217;

bool tgEnabled = true;
String tgBotToken = "1234567890:AAHtest";
String tgChatId = "123456789";
bool littlefsReady = true;
volatile size_t g_histShortSize = 0;
volatile size_t g_histLongSize = 0;
SemaphoreHandle_t logMutex = nullptr;
SemaphoreHandle_t tgSendMutex = nullptr;
WiFiClientSecure tgClient;
bool tgClientReady = false;
unsigned long tgClientLastUse = 0;

bool g_polaczenieOk211 = true;
bool g_guardOk211 = true;
bool g_sendMutexOk211 = true;
int g_netFail211 = 0;
int g_serwis211 = 0;
std::string g_ostatniTag211;
bool g_historiaGotowa211 = true;
const char *HISTORIA_TOKEN = "token-testowy-0123456789";
std::uint32_t g_los213 = 1;

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

void diagEmit(int cat, int level, const char *msg) {
  (void)cat;
  if (level == DL_ERR) ++g_diagErr211;
  if (level == DL_WARN) ++g_diagWarn211;
  g_diag217.push_back(msg ? msg : "");
}

// --- PRAWDZIWY kod z pliku po patchu v3.56.3 ----------------------------
#define malloc(x) model_malloc(x)
#include "Extracted217.inc"
#undef malloc

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

int g_maxOtwartych = 0;
void pilnujUchwytow() {
  if (g_ram217.otwartych > g_maxOtwartych) g_maxOtwartych = g_ram217.otwartych;
}

std::string zrobPlik(int linii, const char *znacznik) {
  std::string s = "czas,wartosc\n";
  for (int i = 0; i < linii; ++i) {
    s += std::to_string(i) + "," + znacznik + "-" + std::to_string(i * 7 % 1000) + "\n";
  }
  return s;
}

// ~5 KB: kilka porcji po 97 B, wiec "zerwanie w polowie" i "blad VFS w trakcie"
// maja co przerwac.
const std::string kPlikKrotki = zrobPlik(500, "krotka");
const std::string kPlikDlugi = zrobPlik(6, "dluga");

void przygotuj() {
  g_ram217.wyczysc();
  g_zegar211 = 1000;
  g_los213 = 7;
  g_blokadaZajeta211 = false;
  g_psmallocFail211 = 0;
  g_mallocFail211 = 0;
  g_polaczenieOk211 = true;
  g_guardOk211 = true;
  g_sendMutexOk211 = true;
  g_netFail211 = 0;
  g_historiaGotowa211 = true;
  littlefsReady = true;
  tgEnabled = true;
  g_histShortSize = 0;
  g_histLongSize = 0;
  bilety.wyczysc();
  g_ram217.utworz(historia211::kShortVfsPath, kPlikKrotki);
  g_ram217.utworz(historia211::kLongVfsPath, kPlikDlugi);
  g_histShortSize = kPlikKrotki.size();
  g_histLongSize = kPlikDlugi.size();
  tgClient.odpowiedz = "HTTP/1.1 200 OK\r\n\r\n{\"ok\":true}";
  tgClient.zerowyZapisPo = -1;
  tgClient.limitZapisu = 0;
}

struct Wynik {
  int kod = 0;
  AsyncWebServerResponse *odpowiedz = nullptr;
  std::string cialo;
};

using Obsluz = void (*)(AsyncWebServerRequest *);

Obsluz obslugaDla(const char *sciezka) {
  for (int i = 0; i < TRASY_OBSLUGI217_N; ++i) {
    if (std::strcmp(sciezka, TRASY_OBSLUGI217[i].sciezka) == 0) return TRASY_OBSLUGI217[i].obsluz;
  }
  return nullptr;
}

// Bilet z PRAWDZIWEJ trasy /history/link.
std::string nowyBilet(const char *strumien) {
  AsyncWebServerRequest r;
  r.metoda = HTTP_GET;
  r.dodajParametr("stream", strumien);
  obslugaHistoryLink(&r);
  const std::string bilet = r.beginCialo;
  const int kod = r.kod;
  delete r.wyslana;
  if (kod != 200 || bilet.size() != historia213::kZnakow) return std::string();
  return bilet;
}

// Pobranie z PRAWDZIWEJ trasy. Adres taki, jaki otwiera panel (etap 216).
Wynik pobierz(const char *sciezka, const std::string &bilet) {
  Wynik w;
  Obsluz fn = obslugaDla(sciezka);
  if (fn == nullptr) return w;
  AsyncWebServerRequest r;
  r.metoda = HTTP_GET;
  r.dodajParametr("k", bilet.c_str());
  fn(&r);
  pilnujUchwytow();   // tu odpowiedz (jesli powstala) trzyma otwarta zapinke
  w.kod = r.kod;
  w.odpowiedz = r.wyslana;
  w.cialo = r.beginCialo;
  return w;
}

struct Odbior {
  std::string cialo;
  std::size_t porcji = 0;
  bool zrodloWazne = false;
};

Odbior odbierz217(AsyncAbstractResponse *r, std::size_t bufor = 97, std::size_t maxPorcji = 0) {
  Odbior o;
  std::vector<uint8_t> b(bufor);
  for (std::size_t i = 0; i < 1000000; ++i) {
    if (maxPorcji != 0 && o.porcji >= maxPorcji) break;
    const std::size_t n = r->_fillBuffer(b.data(), b.size());
    if (n == 0) break;
    o.cialo.append(reinterpret_cast<const char *>(b.data()), n);
    ++o.porcji;
  }
  o.zrodloWazne = r->_sourceValid();
  return o;
}

// --- rozbior zadanego ciala multipart z "kabla" Telegramu -----------------
struct Rozbior {
  bool ok = false;
  std::string cialo;        // dokument
  std::size_t ogloszona = 0;  // Content-Length z naglowka
  std::size_t rzeczywista = 0;  // dlugosc ciala w kablu
  std::string nazwaPliku;
};

Rozbior rozbierz(const std::string &kabel) {
  Rozbior w;
  const std::size_t hh = kabel.find("\r\n\r\n");
  if (hh == std::string::npos) return w;
  const std::string naglowki = kabel.substr(0, hh);
  const std::string cialo = kabel.substr(hh + 4);
  w.rzeczywista = cialo.size();

  const std::size_t cl = naglowki.find("Content-Length: ");
  if (cl == std::string::npos) return w;
  const std::size_t koniec = naglowki.find("\r\n", cl);
  w.ogloszona = static_cast<std::size_t>(std::strtoull(naglowki.c_str() + cl + 16, nullptr, 10));
  (void)koniec;

  const std::size_t b = naglowki.find("boundary=");
  if (b == std::string::npos) return w;
  const std::size_t bk = naglowki.find("\r\n", b);
  const std::string boundary = naglowki.substr(b + 9, bk - (b + 9));

  const std::string ogon = "\r\n--" + boundary + "--\r\n";
  if (cialo.size() < ogon.size() || cialo.compare(cialo.size() - ogon.size(), ogon.size(), ogon) != 0) {
    return w;
  }
  const std::size_t fn = cialo.find("filename=\"");
  if (fn != std::string::npos) {
    const std::size_t k2 = cialo.find('"', fn + 10);
    if (k2 != std::string::npos) w.nazwaPliku = cialo.substr(fn + 10, k2 - (fn + 10));
  }
  const std::string ozn = "Content-Type: text/csv\r\n\r\n";
  const std::size_t p = cialo.find(ozn);
  if (p == std::string::npos) return w;
  const std::size_t od = p + ozn.size();
  const std::size_t doK = cialo.size() - ogon.size();
  if (doK < od) return w;
  w.cialo = cialo.substr(od, doK - od);
  w.ok = true;
  return w;
}

void sprawdzWysylke(const std::string &kabel, const std::string &plik, const char *nazwa) {
  const Rozbior w = rozbierz(kabel);
  OK(w.ok);
  OK(w.ogloszona == w.rzeczywista);   // zapowiedziane = wyslane (obietnica z 211)
  OK(w.cialo == plik);
  OK(w.nazwaPliku == nazwa);
}
}  // namespace

int main() {
  logMutex = xSemaphoreCreateMutex();
  tgSendMutex = xSemaphoreCreateMutex();
  historiaPobranie.ustawBilety(&bilety);

  // ---- G01: 300 pobran HTTP pod rzad --------------------------------
  grupa("G01 300 pobran HTTP: 0 wyciekow uchwytow, zawszy wlasciwy plik");
  {
    przygotuj();
    int zlyKod = 0, zleCialo = 0, wycieki = 0, zajete = 0;
    for (int i = 0; i < 300; ++i) {
      const std::string bilet = nowyBilet("short");
      if (bilet.empty()) { ++zlyKod; continue; }
      Wynik w = pobierz("/history", bilet);
      if (w.kod != 200) ++zlyKod;
      AsyncAbstractResponse *s = w.odpowiedz ? w.odpowiedz->jakoStrumien() : nullptr;
      if (s != nullptr) {
        const Odbior o = odbierz217(s);
        if (o.cialo != kPlikKrotki) ++zleCialo;
      } else {
        ++zleCialo;
      }
      delete w.odpowiedz;   // biblioteka usuwa odpowiedz po wysylce/zerwaniu
      pilnujUchwytow();
      if (g_ram217.otwartych != 0) ++wycieki;
      if (historiaPobranie.zajeta()) ++zajete;
    }
    OK(zlyKod == 0);
    OK(zleCialo == 0);
    OK(wycieki == 0);
    OK(zajete == 0);
    OK(g_ram217.otwartych == 0);
    OK(g_ram217.otwarciem == g_ram217.zamkniec);
    OK(g_maxOtwartych <= 1);
  }

  // ---- G02: 50 zerwanych pobran w polowie ---------------------------
  grupa("G02 50 zerwanych pobran w polowie: uchwyt zwolniony, flaga czysta");
  {
    przygotuj();
    int wycieki = 0, zajete = 0, zlePorcje = 0;
    for (int i = 0; i < 50; ++i) {
      const std::string bilet = nowyBilet("short");
      Wynik w = pobierz("/history", bilet);
      AsyncAbstractResponse *s = w.odpowiedz ? w.odpowiedz->jakoStrumien() : nullptr;
      if (s != nullptr) {
        const Odbior o = odbierz217(s, 97, 1);   // jedna porcja i klient znika
        if (o.cialo.size() != 97) ++zlePorcje;
      }
      delete w.odpowiedz;
      pilnujUchwytow();
      if (g_ram217.otwartych != 0) ++wycieki;
      if (historiaPobranie.zajeta()) ++zajete;
    }
    OK(zlePorcje == 0);
    OK(wycieki == 0);
    OK(zajete == 0);
    const std::string bilet = nowyBilet("short");
    Wynik w = pobierz("/history", bilet);
    OK(w.kod == 200);
    AsyncAbstractResponse *s = w.odpowiedz ? w.odpowiedz->jakoStrumien() : nullptr;
    if (s != nullptr) OK(odbierz217(s).cialo == kPlikKrotki);
    delete w.odpowiedz;
  }

  // ---- G03: dwa pobrania naraz --------------------------------------
  grupa("G03 drugie pobranie naraz: 503, bilet NIE spalony, po pierwszym dziala");
  {
    przygotuj();
    const std::string biletA = nowyBilet("short");
    const std::string biletB = nowyBilet("short");
    Wynik a = pobierz("/history", biletA);
    OK(a.kod == 200);
    OK(historiaPobranie.zajeta());
    Wynik b = pobierz("/history", biletB);
    OK(b.kod == 503);
    OK(b.cialo.find("Trwa inne pobranie") != std::string::npos);
    delete b.odpowiedz;
    AsyncAbstractResponse *s = a.odpowiedz ? a.odpowiedz->jakoStrumien() : nullptr;
    if (s != nullptr) OK(odbierz217(s).cialo == kPlikKrotki);
    delete a.odpowiedz;
    OK(!historiaPobranie.zajeta());
    Wynik b2 = pobierz("/history", biletB);   // bilet przelezal odrzucenie
    OK(b2.kod == 200);
    AsyncAbstractResponse *s2 = b2.odpowiedz ? b2.odpowiedz->jakoStrumien() : nullptr;
    if (s2 != nullptr) OK(odbierz217(s2).cialo == kPlikKrotki);
    delete b2.odpowiedz;
    pilnujUchwytow();
    OK(g_ram217.otwartych == 0);
  }

  // ---- G04: VFS zawodzi w polowie odczytu ----------------------------
  grupa("G04 blad VFS w trakcie: zrodlo niewazne, uchwyt zwolniony, nastepne OK");
  {
    przygotuj();
    g_ram217.awariaOdczytuPo = 200;
    const std::string bilet = nowyBilet("short");
    Wynik w = pobierz("/history", bilet);
    OK(w.kod == 200);
    AsyncAbstractResponse *s = w.odpowiedz ? w.odpowiedz->jakoStrumien() : nullptr;
    if (s != nullptr) {
      const Odbior o = odbierz217(s);
      OK(!o.zrodloWazne);          // strumien nie udaje, ze wszystko poszlo
      OK(o.cialo.size() < kPlikKrotki.size());
    }
    delete w.odpowiedz;
    pilnujUchwytow();
    OK(g_ram217.otwartych == 0);
    OK(!historiaPobranie.zajeta());
    g_ram217.awariaOdczytuPo = -1;
    const std::string b2 = nowyBilet("short");
    Wynik w2 = pobierz("/history", b2);
    OK(w2.kod == 200);
    AsyncAbstractResponse *s2 = w2.odpowiedz ? w2.odpowiedz->jakoStrumien() : nullptr;
    if (s2 != nullptr) OK(odbierz217(s2).cialo == kPlikKrotki);
    delete w2.odpowiedz;
  }

  // ---- G05: blokada pisarza zajeta ----------------------------------
  grupa("G05 logMutex zajety: 503, zero uchwytow, flaga czysta");
  {
    przygotuj();
    g_blokadaZajeta211 = true;
    const std::string bilet = nowyBilet("short");
    Wynik w = pobierz("/history", bilet);
    OK(w.kod == 503);
    OK(w.cialo.find("Zapis historii chwilowo zajety") != std::string::npos);
    delete w.odpowiedz;
    OK(g_ram217.otwartych == 0);
    OK(!historiaPobranie.zajeta());
    g_blokadaZajeta211 = false;
  }

  // ---- G06: 200 wysylek Telegram -----------------------------------
  grupa("G06 200 wysylek hist_short do Telegramu: cialo i Content-Length zgodne");
  {
    przygotuj();
    std::string plik;
    for (int i = 0; i < 700; ++i) plik += "linia," + std::to_string(i) + "\n";
    g_ram217.utworz(historia211::kShortVfsPath, plik);
    g_histShortSize = plik.size();
    int nieOk = 0, wycieki = 0, zleCialo = 0, zlaDlugosc = 0, zlaNazwa = 0;
    for (int i = 0; i < 200; ++i) {
      tgClient.odpowiedz = "HTTP/1.1 200 OK\r\n\r\n{\"ok\":true}";
      if (!sendTelegramDocumentHistKrotka()) ++nieOk;
      const Rozbior w = rozbierz(tgClient.kabel);
      if (!w.ok || w.cialo != plik) ++zleCialo;
      if (w.ogloszona != w.rzeczywista) ++zlaDlugosc;
      if (w.nazwaPliku != "hist_short.csv") ++zlaNazwa;
      pilnujUchwytow();
      if (g_ram217.otwartych != 0) ++wycieki;
    }
    OK(nieOk == 0);
    OK(zleCialo == 0);
    OK(zlaDlugosc == 0);
    OK(zlaNazwa == 0);
    OK(wycieki == 0);
    OK(g_ram217.otwartych == 0);
  }

  // ---- G07: sciezki awarii wysylki ---------------------------------
  grupa("G07 awarie wysylki Telegram: kazda konczy sie zerwaniem, nie 'udanym' plikiem");
  {
    przygotuj();
    // (a) krotki zapis do gniazda
    tgClient.zerowyZapisPo = 1;
    OK(!sendTelegramDocumentHistKrotka());
    OK(g_netFail211 > 0);
    OK(g_ram217.otwartych == 0);

    // (b) brak pamieci na bufor 4 KB
    przygotuj();
    g_psmallocFail211 = 1;
    g_mallocFail211 = 1;
    OK(!sendTelegramDocumentHistKrotka());
    OK(g_netFail211 > 0);
    OK(g_ram217.otwartych == 0);

    // (c) VFS zawodzi w trakcie
    przygotuj();
    g_ram217.awariaOdczytuPo = 200;
    OK(!sendTelegramDocumentHistKrotka());
    OK(g_ram217.otwartych == 0);

    // (d) nic nie potwierdzone (cache 0) - nie ma czego wysylac
    przygotuj();
    g_histShortSize = 0;
    OK(!sendTelegramDocumentHistKrotka());
    OK(g_ram217.otwartych == 0);

    // (e) cache wiekszy niz plik - wysylka przycina sie do fizycznego prefiksu
    przygotuj();
    g_histShortSize = kPlikKrotki.size() + 1000;
    OK(sendTelegramDocumentHistKrotka());
    sprawdzWysylke(tgClient.kabel, kPlikKrotki, "hist_short.csv");
    OK(g_ram217.otwartych == 0);
  }

  // ---- G08: dluga historia ------------------------------------------
  grupa("G08 hist_long: wysylka poprawna, zapinka TooLarge odrzucona bez uchwytu");
  {
    przygotuj();
    OK(sendTelegramDocumentHistDluga());
    sprawdzWysylke(tgClient.kabel, kPlikDlugi, "hist_long.csv");
    OK(g_ram217.otwartych == 0);

    historia211::Pin pin;
    const historia211::OpenCode kod =
        pin.open(historiaSys, historia211::kLongVfsPath, g_histLongSize, 10);
    OK(kod == historia211::OpenCode::TooLarge);
    OK(!pin.active());
    OK(g_ram217.otwartych == 0);
  }

  // ---- G09: podsumowanie zasobow ------------------------------------
  grupa("G09 bilans uchwytow po calym tescie");
  {
    OK(g_ram217.otwartych == 0);
    OK(g_ram217.otwarciem == g_ram217.zamkniec);
    OK(g_maxOtwartych <= 1);
  }

  std::printf("%s wytrzymalosc217 sprawdzen=%d bledow=%d (max otwartych naraz: %d)\n",
              g_bledow == 0 ? "PASS" : "FAIL", g_sprawdzen, g_bledow, g_maxOtwartych);
  return g_bledow == 0 ? 0 : 1;
}
