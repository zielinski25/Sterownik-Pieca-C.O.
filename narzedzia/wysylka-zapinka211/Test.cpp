// 211: test hostowy PRAWDZIWEGO kodu obu wysylaczy dokumentow Telegram
// (wyciagnietych z main_centrala.cpp PO patchu v3.55.9) plus nowej zapinki
// HistoriaPin.h. Modelowany jest wylacznie swiat zewnetrzny (gniazdo, czas,
// semafory, VFS, alokacja). Zaden fragment funkcji wysylajacych nie jest
// przepisany - Extracted211.inc jest generowany przez build.py.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Arduino.h"
#include "RamCalls.h"
#include "Firmware211.h"
#include "HistoriaPin.h"

// --- globalne obiekty "firmware'u" (modele) ------------------------------
unsigned long g_zegar211 = 0;
int g_psmallocFail211 = 0;
int g_mallocFail211 = 0;
int g_diagErr211 = 0;
int g_diagWarn211 = 0;
std::vector<std::string> g_diag211;
bool g_blokadaZajeta211 = false;
WiFiClass WiFi;
SerialClass Serial;

RamCalls g_ram211;
history193::Calls &historiaSys = g_ram211;

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

// --- implementacje modeli ------------------------------------------------
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

void SerialClass::printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);
}
void SerialClass::println(const char *s) { std::fprintf(stderr, "%s\n", s ? s : ""); }

void diagEmit(int cat, int level, const char *msg) {
  (void)cat;
  if (level == DL_ERR) ++g_diagErr211;
  if (level == DL_WARN) ++g_diagWarn211;
  g_diag211.push_back(msg ? msg : "");
}

// --- PRAWDZIWY kod po patchu --------------------------------------------
#define malloc(x) model_malloc(x)
#include "Extracted211.inc"
#undef malloc

// --- ramka testowa -------------------------------------------------------
namespace {
int g_grupa = 0;
int g_sprawdzen = 0;
int g_bledow = 0;
const char *g_nazwaGrupy = "";

void grupa(const char *n) {
  g_nazwaGrupy = n;
  ++g_grupa;
  std::printf("-- %02d %s\n", g_grupa, n);
}

void sprawdz(bool warunek, const char *co, int linia) {
  ++g_sprawdzen;
  if (!warunek) {
    ++g_bledow;
    std::printf("FAIL [%s] %s (Test.cpp:%d)\n", g_nazwaGrupy, co, linia);
  }
}
#define OK(x) sprawdz((x), #x, __LINE__)

void przygotuj() {
  g_ram211.wyczysc();
  g_zegar211 = 0;
  g_psmallocFail211 = 0;
  g_mallocFail211 = 0;
  g_diagErr211 = 0;
  g_diagWarn211 = 0;
  g_diag211.clear();
  g_blokadaZajeta211 = false;
  WiFi.ustaw(WL_CONNECTED);
  tgEnabled = true;
  tgBotToken = "1234567890:AAHtest";
  tgChatId = "123456789";
  littlefsReady = true;
  g_historiaGotowa211 = true;
  g_polaczenieOk211 = true;
  g_guardOk211 = true;
  g_sendMutexOk211 = true;
  g_netFail211 = 0;
  g_serwis211 = 0;
  g_ostatniTag211.clear();
  tgClient = WiFiClientSecure();
  tgClient.odpowiedz = "{\"ok\":true,\"result\":{}}";
  tgClientReady = false;
  tgClientLastUse = 0;
}

std::string powtorz(char c, size_t n) { return std::string(n, c); }

// Wytnij cialo dokumentu z kabla: miedzy koncem partHead a partTail.
bool cialo(const std::string &kabel, const std::string &ogon, std::string &wynik) {
  const std::string marker = "Content-Type: text/csv\r\n\r\n";
  const size_t start = kabel.find(marker);
  if (start == std::string::npos) return false;
  const size_t od = start + marker.size();
  if (kabel.size() < od + ogon.size()) return false;
  if (kabel.compare(kabel.size() - ogon.size(), ogon.size(), ogon) != 0) return false;
  wynik = kabel.substr(od, kabel.size() - od - ogon.size());
  return true;
}

// Content-Length z ogona naglowka HTTP (nie z ciala).
bool dlugoscZNaglowka(const std::string &kabel, size_t &n) {
  const std::string klucz = "Content-Length: ";
  const size_t p = kabel.find(klucz);
  if (p == std::string::npos) return false;
  const size_t k = kabel.find("\r\n", p);
  if (k == std::string::npos) return false;
  n = static_cast<size_t>(std::stoull(kabel.substr(p + klucz.size(), k - p - klucz.size())));
  return true;
}

bool wyslanoDokument(const std::string &kabel, const char *ogon, std::string &cialoOut) {
  std::string c;
  if (!cialo(kabel, ogon, c)) return false;
  cialoOut = c;
  return true;
}
}  // namespace

// ========================================================================
int main() {
  logMutex = xSemaphoreCreateMutex();
  tgSendMutex = xSemaphoreCreateMutex();
  const std::string ogonS = "\r\n--ESP32TGBndHistS1--\r\n";
  const std::string ogonL = "\r\n--ESP32TGBndHistL1--\r\n";

  // ---- 01: zwykla wysylka - dokladnie tyle bajtow, ile zapowiedziano ----
  grupa("01 zwykla wysylka: cialo == plik, zapowiedz == cialo");
  {
    przygotuj();
    const std::string tresc = "a,x\nb,y\nc,z\n";
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    const bool ok = sendTelegramDocumentHistKrotka();
    std::string c;
    OK(ok);
    OK(wyslanoDokument(tgClient.kabel, ogonS.c_str(), c));
    OK(c == tresc);
    size_t zapowiedziano = 0;
    OK(dlugoscZNaglowka(tgClient.kabel, zapowiedziano));
    OK(zapowiedziano == tgClient.kabel.size() - tgClient.kabel.find("\r\n\r\n") - 4);
    OK(tgClient.kabel.rfind("POST /bot1234567890:AAHtest/sendDocument HTTP/1.1\r\n", 0) == 0);
    OK(tgClient.kabel.find("filename=\"hist_short.csv\"") != std::string::npos);
    OK(tgClient.kabel.find("(0 KB)") != std::string::npos);
    OK(g_ram211.otwartych == 0);
    OK(g_ram211.otwarciem == 1);
    OK(g_ostatniTag211 == "TG-DOC-HIST");
  }

  // ---- 02: cache wiekszy niz plik (dzisiejsza pułapka) -----------------
  grupa("02 cache > plik: zapowiedz i cialo biora sie z pliku");
  {
    przygotuj();
    const std::string tresc = powtorz('q', 40);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = 1000;  // znacznik zapisu "wysunięty" względem pliku
    const bool ok = sendTelegramDocumentHistKrotka();
    std::string c;
    size_t zapowiedziano = 0;
    OK(ok);
    OK(wyslanoDokument(tgClient.kabel, ogonS.c_str(), c));
    OK(c.size() == 40);
    OK(c == tresc);
    OK(dlugoscZNaglowka(tgClient.kabel, zapowiedziano));
    OK(zapowiedziano == tgClient.kabel.size() - tgClient.kabel.find("\r\n\r\n") - 4);
  }

  // ---- 03: cache mniejszy niz plik -------------------------------------
  grupa("03 cache < plik: wysylamy tylko potwierdzony prefiks");
  {
    przygotuj();
    const std::string tresc = powtorz('r', 100);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = 60;
    OK(sendTelegramDocumentHistKrotka());
    std::string c;
    OK(wyslanoDokument(tgClient.kabel, ogonS.c_str(), c));
    OK(c.size() == 60);
    OK(c == tresc.substr(0, 60));
  }

  // ---- 04: brak pliku ---------------------------------------------------
  grupa("04 brak pliku: zero bajtow na kabel");
  {
    przygotuj();
    g_histShortSize = 100;
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
    OK(g_ram211.otwartych == 0);  // probowal otworzyc, dostal ENOENT - uchwatu nie ma
    OK(g_diagWarn211 == 1);
  }

  // ---- 05: pusty plik ---------------------------------------------------
  grupa("05 pusty plik: odmowa bez wysylki");
  {
    przygotuj();
    g_ram211.utworz("/littlefs/hist_short.csv", "");
    g_histShortSize = 0;
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
  }

  // ---- 06: niegotowy magazyn / brak LittleFS ----------------------------
  grupa("06 magazyn niegotowy, brak LittleFS, brak sieci - bez wysylki");
  {
    przygotuj();
    g_ram211.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    g_historiaGotowa211 = false;
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
    przygotuj();
    g_ram211.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    littlefsReady = false;
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
    przygotuj();
    g_ram211.utworz("/littlefs/hist_short.csv", "x\n");
    g_histShortSize = 2;
    WiFi.ustaw(0);
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
  }

  // ---- 07: blokada pisarza zajeta --------------------------------------
  grupa("07 logMutex zajety: ani jednego bajtu, plik nietkniety");
  {
    przygotuj();
    const std::string tresc = "abc\n";
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    g_blokadaZajeta211 = true;
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
    OK(g_ram211.otwarciem == 0);
    OK(g_ram211.tresc("/littlefs/hist_short.csv") == tresc);
  }

  // ---- 08: dopisywanie w trakcie wysylki -------------------------------
  grupa("08 dopisywanie w trakcie wysylki nie zmienia ciala");
  {
    przygotuj();
    const std::string tresc = powtorz('d', 5000);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    size_t dopisane = 0;
    tgClient.poZapisie = [&](size_t) {
      if (dopisane < 3) {
        g_ram211.dopisz("/littlefs/hist_short.csv", "NOWE-DANE\n");
        ++dopisane;
      }
    };
    OK(sendTelegramDocumentHistKrotka());
    std::string c;
    OK(wyslanoDokument(tgClient.kabel, ogonS.c_str(), c));
    OK(c == tresc);
    OK(c.size() == 5000);
    OK(dopisane == 3);
    OK(g_ram211.rozmiar("/littlefs/hist_short.csv") == 5000 + 3 * 10);
  }

  // ---- 09: plik skrocony PRZED pierwszym odczytem ----------------------
  grupa("09 plik krotszy niz zapowiedziano: zerwanie, nie 'udane' uciecie");
  {
    przygotuj();
    const std::string tresc = powtorz('s', 400);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    const std::string marker = "Content-Type: text/csv\r\n\r\n";
    tgClient.poZapisie = [&](size_t) {
      if (tgClient.kabel.find(marker) != std::string::npos)
        g_ram211.przytnij("/littlefs/hist_short.csv", 100);
    };
    const bool ok = sendTelegramDocumentHistKrotka();
    std::string c;
    OK(!ok);
    OK(tgClient.kabel.find("--ESP32TGBndHistS1--") == std::string::npos);  // brak ogona
    OK(cialo(tgClient.kabel, "\r\n--ESP32TGBndHistS1--\r\n", c) == false);
    OK(g_netFail211 == 1);
    OK(g_diagErr211 >= 1);
    OK(g_ram211.otwartych == 0);
  }

  // ---- 09b: plik skrocony w POLOWIE ciala ------------------------------
  grupa("09b plik skrocony po pierwszej porcji ciala: druga porcja to Short");
  {
    przygotuj();
    const std::string tresc = powtorz('t', 9000);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    const std::string marker = "Content-Type: text/csv\r\n\r\n";
    size_t ciala = 0;
    bool przytor = false;
    tgClient.poZapisie = [&](size_t k) {
      if (przytor || tgClient.kabel.find(marker) == std::string::npos) return;
      ciala += k;
      if (ciala >= 4096) {
        przytor = true;
        g_ram211.przytnij("/littlefs/hist_short.csv", 5000);
      }
    };
    const bool ok = sendTelegramDocumentHistKrotka();
    OK(!ok);
    OK(przytor);
    OK(tgClient.kabel.find("--ESP32TGBndHistS1--") == std::string::npos);
    OK(g_netFail211 == 1);
    OK(g_ram211.otwartych == 0);
  }

  // ---- 10: blad odczytu VFS --------------------------------------------
  grupa("10 blad odczytu VFS: zerwanie i TG_NET_FAIL, nie ciche uciecie");
  {
    przygotuj();
    const std::string tresc = powtorz('e', 4096 + 100);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    g_ram211.awariaOdczytuPo = 2000;
    const bool ok = sendTelegramDocumentHistKrotka();
    OK(!ok);
    OK(tgClient.kabel.find("--ESP32TGBndHistS1--") == std::string::npos);
    OK(g_netFail211 == 1);
    OK(g_diagErr211 >= 1);
    OK(g_ram211.otwartych == 0);
  }

  // ---- 11: krotki zapis do gniazda -------------------------------------
  grupa("11 krotki zapis do gniazda (limit 7 B na raz): cialo nadal dokladne");
  {
    przygotuj();
    const std::string tresc = powtorz('w', 1000);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    tgClient.limitZapisu = 7;
    OK(sendTelegramDocumentHistKrotka());
    std::string c;
    OK(wyslanoDokument(tgClient.kabel, ogonS.c_str(), c));
    OK(c == tresc);
  }

  // ---- 12: gniazdo przestaje przyjmowac w polowie ----------------------
  grupa("12 gniazdo przestaje przyjmowac: cialo przerwane, brak udanego konca");
  {
    przygotuj();
    const std::string tresc = powtorz('f', 9000);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    tgClient.zerowyZapisPo = 4500;
    const bool ok = sendTelegramDocumentHistKrotka();
    OK(!ok);
    OK(tgClient.kabel.find("--ESP32TGBndHistS1--") == std::string::npos);
    OK(g_netFail211 == 1);
    OK(tgClient.kabel.size() >= 4500);
    OK(g_ram211.otwartych == 0);
  }

  // ---- 13: brak pamieci na bufor 4 KB ----------------------------------
  grupa("13 brak pamieci na bufor: cialo nie jedzie, zapinka zamknieta");
  {
    przygotuj();
    const std::string tresc = powtorz('m', 500);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    g_psmallocFail211 = 1;
    g_mallocFail211 = 1;
    const bool ok = sendTelegramDocumentHistKrotka();
    OK(!ok);
    OK(tgClient.kabel.find("--ESP32TGBndHistS1--") == std::string::npos);
    OK(g_netFail211 == 1);
    OK(g_ram211.otwartych == 0);
  }

  // ---- 14: nieudane polaczenie -----------------------------------------
  grupa("14 brak polaczenia: zero ciala, zapinka zamknieta");
  {
    przygotuj();
    const std::string tresc = "z\n";
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    g_polaczenieOk211 = false;
    const bool ok = sendTelegramDocumentHistKrotka();
    OK(!ok);
    OK(tgClient.kabel.empty());
    OK(g_ram211.otwartych == 0);
  }

  // ---- 15: zajety tgSendMutex ------------------------------------------
  grupa("15 tgSendMutex zajety: zero bajtow, zapinka nie zostaje otwarta");
  {
    przygotuj();
    const std::string tresc = "y\n";
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    g_sendMutexOk211 = false;
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
    OK(g_ram211.otwartych == 0);
  }

  // ---- 16: za duzy plik ------------------------------------------------
  grupa("16 plik powyzej limitu Telegrama: odmowa przed zapowiedzia");
  {
    przygotuj();
    const size_t n = 50u * 1024u * 1024u + 1u;
    g_ram211.utworz("/littlefs/hist_short.csv", powtorz('L', n));
    g_histShortSize = n;
    OK(!sendTelegramDocumentHistKrotka());
    OK(tgClient.kabel.empty());
    OK(g_ram211.otwartych == 0);
  }

  // ---- 17: odpowiedz Telegramu bez "ok":true --------------------------
  grupa("17 odpowiedz bez ok:true: cialo poszlo, wynik negatywny");
  {
    przygotuj();
    const std::string tresc = "abc\n";
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    tgClient.odpowiedz = "{\"ok\":false,\"description\":\"bad\"}";
    OK(!sendTelegramDocumentHistKrotka());
    std::string c;
    OK(wyslanoDokument(tgClient.kabel, ogonS.c_str(), c));
    OK(c == tresc);
  }

  // ---- 18: historia dluga ----------------------------------------------
  grupa("18 historia dluga: wlasny plik, wlasny ogon, wlasny tag");
  {
    przygotuj();
    const std::string dluga = powtorz('L', 777);
    g_ram211.utworz("/littlefs/hist_long.csv", dluga);
    g_ram211.utworz("/littlefs/hist_short.csv", "krotka\n");
    g_histLongSize = dluga.size();
    g_histShortSize = 7;
    OK(sendTelegramDocumentHistDluga());
    std::string c;
    OK(wyslanoDokument(tgClient.kabel, ogonL.c_str(), c));
    OK(c == dluga);
    OK(tgClient.kabel.find("filename=\"hist_long.csv\"") != std::string::npos);
    OK(tgClient.kabel.find("ostatnie 7 dni") != std::string::npos);
    OK(g_ostatniTag211 == "TG-DOC-HISTL");
  }

  // ---- 19: zapinka otwarta pod logMutexem, gniazdo juz bez niego -------
  grupa("19 logMutex: trzymany przy otwarciu, wolny przy wysylce");
  {
    przygotuj();
    const std::string tresc = powtorz('k', 300);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    bool podBlokada = false;
    bool wolnyPrzyWysylce = false;
    g_ram211.przyOtwarciu = [&](const char *) {
      if (xSemaphoreTake(logMutex, 0) == pdTRUE) {
        xSemaphoreGive(logMutex);
        podBlokada = false;
      } else {
        podBlokada = true;
      }
    };
    tgClient.poZapisie = [&](size_t) {
      if (wolnyPrzyWysylce) return;
      if (xSemaphoreTake(logMutex, 0) == pdTRUE) {
        xSemaphoreGive(logMutex);
        wolnyPrzyWysylce = true;
      }
    };
    OK(sendTelegramDocumentHistKrotka());
    OK(podBlokada);
    OK(wolnyPrzyWysylce);
  }

  // ---- 20: 50 wysylek pod rzad ----------------------------------------
  grupa("20 50 wysylek pod rzad: brak wycieku uchwytow, zawsze ten sam wynik");
  {
    przygotuj();
    const std::string tresc = powtorz('p', 123);
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    int udane = 0;
    for (int i = 0; i < 50; ++i) {
      tgClient.kabel.clear();
      if (sendTelegramDocumentHistKrotka()) ++udane;
    }
    OK(udane == 50);
    OK(g_ram211.otwartych == 0);
    OK(g_ram211.otwarciem == 50);
    OK(g_ram211.zamkniec == 50);
  }

  // ---- 21: blad zamkniecia VFS ----------------------------------------
  grupa("21 blad zamkniecia zapinki: wysylka konczy sie, blad tylko logowany");
  {
    przygotuj();
    const std::string tresc = "abc\n";
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    g_histShortSize = tresc.size();
    g_ram211.awariaZamkniecia = true;
    OK(sendTelegramDocumentHistKrotka());
    OK(g_diagWarn211 >= 1);
    OK(g_ram211.otwartych == 0);
  }

  // ---- 22: zapinka - umowa interfejsu ---------------------------------
  grupa("22 zapinka: End idempotentny, po zamknieciu Fault, drugie open = Fault");
  {
    przygotuj();
    const std::string tresc = "12345";
    g_ram211.utworz("/littlefs/hist_short.csv", tresc);
    historia211::Pin pin;
    OK(pin.open(g_ram211, historia211::kShortVfsPath, 5, historia211::kMaxDocumentBytes) ==
       historia211::OpenCode::Ok);
    OK(pin.length() == 5);
    OK(pin.open(g_ram211, historia211::kShortVfsPath, 5, historia211::kMaxDocumentBytes) ==
       historia211::OpenCode::Fault);
    char bufor[8] = {};
    auto r = pin.read(bufor, 8);
    OK(r.code == historia211::ReadCode::Data && r.bytes == 5);
    OK(std::memcmp(bufor, "12345", 5) == 0);
    OK(pin.sent() == 5);
    r = pin.read(bufor, 8);
    OK(r.code == historia211::ReadCode::End && r.bytes == 0);
    OK(pin.close());
    OK(pin.close());  // powtorne zamkniecie jest bezpieczne
    r = pin.read(bufor, 8);
    OK(r.code == historia211::ReadCode::Fault);
  }

  // ---- 23: zapinka - odmowy -------------------------------------------
  grupa("23 zapinka: brak pliku, zero potwierdzone, za duzo, zly argument");
  {
    przygotuj();
    historia211::Pin p1;
    OK(p1.open(g_ram211, "/littlefs/nie_ma.csv", 10, 100) == historia211::OpenCode::Missing);
    g_ram211.utworz("/littlefs/hist_short.csv", "abc");
    historia211::Pin p2;
    OK(p2.open(g_ram211, historia211::kShortVfsPath, 0, 100) ==
       historia211::OpenCode::NothingCommitted);
    historia211::Pin p3;
    OK(p3.open(g_ram211, historia211::kShortVfsPath, 3, 2) == historia211::OpenCode::TooLarge);
    historia211::Pin p4;
    OK(p4.open(g_ram211, nullptr, 3, 100) == historia211::OpenCode::Fault);
    historia211::Pin p5;
    OK(p5.open(g_ram211, historia211::kShortVfsPath, 3, 0) == historia211::OpenCode::Fault);
    OK(g_ram211.otwartych == 0);  // kazda odmowa zamyka uchwyt
  }

  // ---- 24: zapinka - krotszy plik to Short, nie End --------------------
  grupa("24 zapinka: krotszy plik = Short, dalsze uzycie = Fault");
  {
    przygotuj();
    g_ram211.utworz("/littlefs/hist_short.csv", powtorz('x', 100));
    historia211::Pin pin;
    OK(pin.open(g_ram211, historia211::kShortVfsPath, 100, historia211::kMaxDocumentBytes) ==
       historia211::OpenCode::Ok);
    g_ram211.przytnij("/littlefs/hist_short.csv", 10);
    char bufor[64];
    auto r = pin.read(bufor, 64);
    OK(r.code == historia211::ReadCode::Short);
    OK(pin.sent() == 0);
    r = pin.read(bufor, 64);
    OK(r.code == historia211::ReadCode::Fault);
    OK(g_ram211.otwartych == 1);  // zapinka nie zamyka sie sama po Short
    OK(pin.close());
    OK(g_ram211.otwartych == 0);
  }

  // ---- 25: zapinka - blad odczytu to Fault, nie koniec -----------------
  grupa("25 zapinka: blad VFS przy odczycie = Fault, nigdy End");
  {
    przygotuj();
    g_ram211.utworz("/littlefs/hist_short.csv", powtorz('x', 100));
    historia211::Pin pin;
    OK(pin.open(g_ram211, historia211::kShortVfsPath, 100, historia211::kMaxDocumentBytes) ==
       historia211::OpenCode::Ok);
    g_ram211.awariaOdczytuPo = 0;  // pierwszy read zwraca -1/EIO
    char bufor[16];
    auto r = pin.read(bufor, 16);
    OK(r.code == historia211::ReadCode::Fault);
    OK(pin.sent() == 0);
    r = pin.read(bufor, 16);
    OK(r.code == historia211::ReadCode::Fault);  // zapinka zepsuta do konca
    OK(pin.close());
    OK(g_ram211.otwartych == 0);
  }

  std::printf("\n%s grup=%d sprawdzen=%d bledow=%d\n",
              g_bledow == 0 ? "PASS route211" : "FAIL route211", g_grupa, g_sprawdzen, g_bledow);
  return g_bledow == 0 ? 0 : 1;
}
