// ============================================================================
// Testy na PC kodu [v3.58.6 PANEL-OTA] centrali: posrednictwo aktualizacji
// panelu LCD (/update_panel, GET /api/status, GET /panel-ota.bin).
//
// Testowany jest tekst WYCIETY z ../src/main_centrala.cpp przez
// "wytnij_blok.py --ota" (plik panel_ota_wycinek.inc powstaje przy kazdym
// uruchomieniu testow), a nie jego kopia: blok decyzji, stan i kod klejacy.
// Ponizej jest tylko otoczenie, ktorego ten kod wymaga od centrali: String,
// sekcja krytyczna, NVS (Preferences), log, Telegram, zapytanie HTTP,
// obecnosc pliku, alarmy i dwie funkcje GitHuba (podstawione).
//
// Atrapy pilnuja tez trzech zasad, ktorych zwykle uruchomienie by nie pokazalo:
//   - w sekcji krytycznej nie ma przydzialu pamieci, logu, NVS ani Telegrama
//     (atrapa::naruszen),
//   - kazda sekcja krytyczna jest zamknieta (atrapa::sekcja == 0),
//   - pamiec robocza zadania Telegrama (calloc/free w kodzie klejacym - tam,
//     zeby teksty nie lezaly na stosie tego zadania) jest zawsze zwalniana,
//     a gdy sterta odmowi, nic nie ginie (atrapa::blokowZywych, odmowPamieci).
//
// CZEGO TU NIE MA: prawdziwego serwera HTTP, prawdziwego NVS, Telegrama,
// GitHuba i samego pobierania pliku na dysk centrali. Nie ma tez STOSU zadania
// Telegrama: ile go zostaje, pokaze dopiero plytka (hwm_tg w logu HEALTH-A)
// (pobierzFirmwarePanelaZGithub() jest tu podstawiona - w centrali uzywa
// HTTPClient i LittleFS). Panelu tez nie: prawdziwy firmware panelu v1.4.0
// rozmawia z blokiem decyzji w testach LCD-Centrali/testy_host.
// Uruchomienie: ./uruchom_testy.sh
// ============================================================================
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Otoczenie (atrapa tego, co w centrali daje Arduino / ESP-IDF i reszta pliku)
// ---------------------------------------------------------------------------
namespace atrapa {
uint32_t terazMs = 1000;
int sekcja = 0;      // glebokosc portENTER_CRITICAL
int naruszen = 0;    // String / log / NVS / Telegram / HTTP uzyte w sekcji krytycznej (ma byc 0)
std::vector<std::string> log;
std::vector<std::string> telegram;
std::map<std::string, std::string> nvsTekst;
std::map<std::string, uint32_t> nvsLiczba;
std::map<std::string, bool> nvsBool;
int zapisowNvs = 0;
// Sterta widziana przez kod klejacy (calloc/free podstawione nizej).
int blokowZywych = 0;        // przydzielone i jeszcze nie zwolnione (po kazdej operacji ma byc 0)
int przydzialow = 0;         // ile razy kod klejacy dostal pamiec
int odmowPamieci = 0;        // tyle kolejnych calloc() zwroci nullptr
size_t najwiekszyBlok = 0;
bool plikNaDysku = false;
// GitHub (podstawione funkcje)
bool githubOk = true;
std::string githubTag = "3.58.6";
std::function<void()> wTrakciePytaniaGithuba;   // to, co dzieje sie w centrali, gdy komenda czeka na odpowiedz GitHuba
bool pobranieOk = true;
std::string pobranieBlad = "blad sieci";
std::vector<uint8_t> plikWydania;
int pobranZGithuba = 0;
int pobranZNiepustymBledem = 0;   // pobierzFirmwarePanelaZGithub() wywolana z niepustym bladOut (ma byc 0)
bool flagaWTrakciePobierania = false;   // otaOperacjaWTrakcie widziane w trakcie pobierania z GitHuba
bool ofertaWTrakciePobierania = false;  // czy w trakcie pobierania z GitHuba panel widzial jeszcze poprzednia oferte
inline void poza() { if (sekcja > 0) naruszen++; }
}   // namespace atrapa

class __FlashStringHelper;
#define F(tekst) (reinterpret_cast<const __FlashStringHelper*>(tekst))

class String {
 public:
  String() { atrapa::poza(); }
  String(const char* s) : t(s ? s : "") { atrapa::poza(); }
  String(const __FlashStringHelper* s) : t(reinterpret_cast<const char*>(s)) { atrapa::poza(); }
  String(const String& o) : t(o.t) { atrapa::poza(); }
  explicit String(unsigned long v) : t(std::to_string(v)) { atrapa::poza(); }
  explicit String(int v) : t(std::to_string(v)) { atrapa::poza(); }
  String& operator=(const String& o) { atrapa::poza(); t = o.t; return *this; }
  String& operator=(const char* s) { atrapa::poza(); t = s ? s : ""; return *this; }
  const char* c_str() const { return t.c_str(); }
  unsigned int length() const { return (unsigned int)t.size(); }
  String& operator+=(const String& o) { atrapa::poza(); t += o.t; return *this; }
  String& operator+=(const char* s) { atrapa::poza(); t += (s ? s : ""); return *this; }
  String& operator+=(const __FlashStringHelper* s) { atrapa::poza(); t += reinterpret_cast<const char*>(s); return *this; }
  bool operator==(const String& o) const { return t == o.t; }
  bool operator==(const char* s) const { return t == (s ? s : ""); }
  bool operator!=(const String& o) const { return t != o.t; }
  bool operator!=(const char* s) const { return t != (s ? s : ""); }
  std::string t;
};
inline String operator+(const String& a, const String& b) { String w(a); w += b; return w; }
inline String operator+(const String& a, const char* b) { String w(a); w += b; return w; }
inline String operator+(const char* a, const String& b) { String w(a); w += b; return w; }

typedef uint8_t byte;
inline uint32_t millis() { return atrapa::terazMs; }

struct portMUX_TYPE { int nieuzywane; };
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(mux) do { (void)(mux); atrapa::sekcja++; } while (0)
#define portEXIT_CRITICAL(mux) do { (void)(mux); atrapa::sekcja--; } while (0)

// NVS: te wywolania Preferences, z ktorych korzysta kod klejacy.
class Preferences {
 public:
  bool getBool(const char* k, bool d) { atrapa::poza(); auto it = atrapa::nvsBool.find(k); return it == atrapa::nvsBool.end() ? d : it->second; }
  size_t putBool(const char* k, bool v) { atrapa::poza(); atrapa::zapisowNvs++; atrapa::nvsBool[k] = v; return 1; }
  uint32_t getUInt(const char* k, uint32_t d) { atrapa::poza(); auto it = atrapa::nvsLiczba.find(k); return it == atrapa::nvsLiczba.end() ? d : it->second; }
  size_t putUInt(const char* k, uint32_t v) { atrapa::poza(); atrapa::zapisowNvs++; atrapa::nvsLiczba[k] = v; return 4; }
  String getString(const char* k, const char* d) { atrapa::poza(); auto it = atrapa::nvsTekst.find(k); return String(it == atrapa::nvsTekst.end() ? d : it->second.c_str()); }
  size_t putString(const char* k, const String& v) { atrapa::poza(); atrapa::zapisowNvs++; atrapa::nvsTekst[k] = v.t; return v.t.size(); }
};
Preferences config;

void zapiszLog(const String& tresc) { atrapa::poza(); atrapa::log.push_back(tresc.t); }
void zapiszLog(const __FlashStringHelper* tresc) { atrapa::poza(); atrapa::log.push_back(reinterpret_cast<const char*>(tresc)); }
bool sendTelegramMessage(const String& text, const String& replyMarkup = String()) {
  (void)replyMarkup;
  atrapa::poza();
  atrapa::telegram.push_back(text.t);
  return true;
}

// System plikow: tylko pytanie o obecnosc pliku i wyslanie go w odpowiedzi.
struct AtrapaFs {
  bool exists(const char* sciezka) { atrapa::poza(); return atrapa::plikNaDysku && std::string(sciezka) == "/panel_firmware.bin"; }
};
AtrapaFs systemFS196;

struct AsyncWebParameter {
  String wartosc;
  const String& value() const { return wartosc; }
};
class AsyncWebServerRequest {
 public:
  std::map<std::string, AsyncWebParameter> parametry;
  int kod = 0;                       // 0 = nic nie wyslano
  bool wyslanoPlik = false;
  std::string tresc;
  std::function<void()> poRozlaczeniu;
  bool hasParam(const char* nazwa) const { return parametry.count(nazwa) != 0; }
  const AsyncWebParameter* getParam(const char* nazwa) const { return &parametry.at(nazwa); }
  void send(int k, const char* typ, const char* t) { (void)typ; atrapa::poza(); kod = k; tresc = t; }
  void send(AtrapaFs&, const char* sciezka, const char* typ) { (void)typ; atrapa::poza(); kod = 200; wyslanoPlik = true; tresc = sciezka; }
  void onDisconnect(std::function<void()> f) { poRozlaczeniu = f; }
};

static const char* OTA_HASLO = "haslo-ota-testowe";
bool alarmOgrzewanieWlaczone = false;
byte czyTemperaturaAlarmowa = 0;
bool alarmPanelWlaczone = false;
byte czyAlarmAktywny = 0;
bool otaOperacjaWTrakcie = false;
volatile size_t g_panelFirmwareSize = 0;

// Sterta: liczy bloki, pilnuje sekcji krytycznej, na zadanie odmawia.
static void* atrapaCalloc(size_t ile, size_t rozmiar) {
  atrapa::poza();
  if (atrapa::odmowPamieci > 0) { atrapa::odmowPamieci--; return nullptr; }
  void* p = std::calloc(ile, rozmiar);
  if (p != nullptr) {
    atrapa::blokowZywych++;
    atrapa::przydzialow++;
    if (ile * rozmiar > atrapa::najwiekszyBlok) atrapa::najwiekszyBlok = ile * rozmiar;
  }
  return p;
}
static void atrapaFree(void* p) {
  atrapa::poza();
  if (p != nullptr) atrapa::blokowZywych--;
  std::free(p);
}
#define calloc(ile, rozmiar) atrapaCalloc((ile), (rozmiar))
#define free(p) atrapaFree(p)

#include "panel_ota_wycinek.inc"

#undef calloc
#undef free

// --- podstawione funkcje GitHuba ---
bool sprawdzNajnowszaWersjeGithub(String& tagOut, String& bladOut) {
  if (atrapa::wTrakciePytaniaGithuba) {
    const std::function<void()> f = atrapa::wTrakciePytaniaGithuba;
    atrapa::wTrakciePytaniaGithuba = nullptr;
    f();
  }
  if (!atrapa::githubOk) { bladOut = "brak internetu"; return false; }
  tagOut = atrapa::githubTag.c_str();
  return true;
}
// Jak w centrali: plik czytany kawalkami po 512 B, kazdy kawalek idzie do
// panelOtaSkanDodaj(); plik odrzucony przez panelOtaSkanOcen() nie zastepuje
// poprzedniego.
bool pobierzFirmwarePanelaZGithub(const String& tag, String& bladOut, PanelOtaSkan& skanOut, size_t& rozmiarOut) {
  (void)tag;
  atrapa::pobranZGithuba++;
  // W centrali ta funkcja po "bladOut.length() == 0" poznaje, czy blad zostal juz opisany.
  if (bladOut.length() != 0) atrapa::pobranZNiepustymBledem++;
  atrapa::flagaWTrakciePobierania = otaOperacjaWTrakcie;
  atrapa::ofertaWTrakciePobierania = g_panelOta.aktywna;
  panelOtaSkanStart(skanOut);
  rozmiarOut = 0;
  if (!atrapa::pobranieOk) { bladOut = atrapa::pobranieBlad.c_str(); return false; }
  for (size_t i = 0; i < atrapa::plikWydania.size(); i += 512) {
    const size_t n = std::min<size_t>(512, atrapa::plikWydania.size() - i);
    panelOtaSkanDodaj(skanOut, atrapa::plikWydania.data() + i, n);
  }
  if (const char* powod = panelOtaSkanOcen(skanOut)) { bladOut = String("plik odrzucony: ") + powod; return false; }
  atrapa::plikNaDysku = true;
  g_panelFirmwareSize = atrapa::plikWydania.size();
  rozmiarOut = atrapa::plikWydania.size();
  return true;
}

// ---------------------------------------------------------------------------
// Narzedzia testu
// ---------------------------------------------------------------------------
static int g_sprawdzen = 0, g_bledow = 0;
static void sprawdz(bool ok, const char* opis) {
  g_sprawdzen++;
  if (!ok) g_bledow++;
  printf("    [%s] %s\n", ok ? " OK " : "BLAD", opis);
}
static bool zawiera(const std::string& s, const char* fragment) { return s.find(fragment) != std::string::npos; }
static std::string ostatniTelegram() { return atrapa::telegram.empty() ? std::string() : atrapa::telegram.back(); }
static bool logMa(const char* fragment) {
  for (const std::string& l : atrapa::log) if (zawiera(l, fragment)) return true;
  return false;
}

// Program ESP32 o dlugosci `n`: naglowek 24 B (uklad `uklad`, znacznik sumy
// `zSuma`), tresc, na koncu 32 bajty "sumy" zaczynajace sie od `ziarno`.
static std::vector<uint8_t> programEsp32(size_t n, uint8_t ziarno, unsigned uklad = 0x0000, bool zSuma = true, uint8_t pierwszy = 0xE9) {
  std::vector<uint8_t> p(n);
  for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(i * 31U + ziarno);
  p[0] = pierwszy;
  p[12] = (uint8_t)(uklad & 0xFF);
  p[13] = (uint8_t)(uklad >> 8);
  p[23] = zSuma ? 1 : 0;
  for (size_t i = 0; i < 32; i++) p[n - 32 + i] = (uint8_t)(ziarno + i * 7U);
  return p;
}
static std::string obrazPliku(const std::vector<uint8_t>& p) {
  char o[17];
  panelOtaObrazZSumy(p.data() + p.size() - 32, o);
  return o;
}

static const uint32_t MINUTA = 60000U, GODZINA = 3600000U;

// "Wlaczenie centrali": pusty RAM, pusty NVS, czas od 1 s.
static void nowaCentrala() {
  atrapa::terazMs = 1000;
  atrapa::log.clear();
  atrapa::telegram.clear();
  atrapa::nvsTekst.clear();
  atrapa::nvsLiczba.clear();
  atrapa::nvsBool.clear();
  atrapa::zapisowNvs = 0;
  atrapa::plikNaDysku = false;
  atrapa::githubOk = true;
  atrapa::githubTag = "3.58.6";
  atrapa::pobranieOk = true;
  atrapa::plikWydania = programEsp32(200000, 0x40);
  atrapa::pobranZGithuba = 0;
  atrapa::odmowPamieci = 0;
  atrapa::wTrakciePytaniaGithuba = nullptr;
  g_panelOta = {};
  g_panelOtaPanel = {};
  g_panelOtaDoWykonania = {};
  alarmOgrzewanieWlaczone = false;
  czyTemperaturaAlarmowa = 0;
  alarmPanelWlaczone = false;
  czyAlarmAktywny = 0;
  otaOperacjaWTrakcie = false;
  g_panelFirmwareSize = 0;
}
// Restart centrali: RAM od nowa, NVS i dysk zostaja; na koncu setup() jak w pliku.
static void restartCentrali() {
  g_panelOta = {};
  g_panelOtaPanel = {};
  g_panelOtaDoWykonania = {};
  otaOperacjaWTrakcie = false;
  atrapa::terazMs = 1000;
  g_panelFirmwareSize = atrapa::plikNaDysku ? atrapa::plikWydania.size() : 0;
  panelOtaKlejPoStarcie();
}

struct PolaJson {
  bool dostepna = false;
  std::string wersja, obraz, caly;
  unsigned long nr = 0;
};
static PolaJson polaJson() {
  String j;
  panelOtaKlejJson(j);
  PolaJson p;
  p.caly = j.t;
  p.dostepna = zawiera(j.t, "\"panel_ota_dostepna\":true");
  auto tekst = [&](const char* klucz) {
    const std::string k = std::string("\"") + klucz + "\":\"";
    const size_t a = j.t.find(k);
    if (a == std::string::npos) return std::string("?");
    const size_t od = a + k.size();
    return j.t.substr(od, j.t.find('"', od) - od);
  };
  p.wersja = tekst("panel_ota_wersja");
  p.obraz = tekst("panel_ota_obraz");
  const size_t a = j.t.find("\"panel_ota_nr\":");
  p.nr = a == std::string::npos ? 999999 : strtoul(j.t.c_str() + a + 15, nullptr, 10);
  return p;
}

// Zapytanie panelu o status - to, co robi trasa GET /api/status w centrali.
static PolaJson status(const char* wersja, std::map<std::string, std::string> dodatkowe = {}) {
  AsyncWebServerRequest zapytanie;
  if (wersja != nullptr) zapytanie.parametry["panel_fw"].wartosc = wersja;
  for (auto& para : dodatkowe) zapytanie.parametry[para.first].wartosc = para.second.c_str();
  if (zapytanie.hasParam("panel_fw")) {
    String zgloszona = zapytanie.getParam("panel_fw")->value();
    panelOtaKlejZgloszenie(&zapytanie, zgloszona);
  }
  return polaJson();
}
// Zapytanie o plik. Zwraca obiekt zapytania (kod, czy wyslano plik, funkcja rozlaczenia).
static AsyncWebServerRequest plik(const char* haslo = "haslo-ota-testowe") {
  AsyncWebServerRequest zapytanie;
  if (haslo != nullptr) zapytanie.parametry["token"].wartosc = haslo;
  panelOtaKlejPlik(&zapytanie);
  return zapytanie;
}
static void obieg() { panelOtaKlejOkresowo(); }
// case TG_UPDATE_PANEL z pliku centrali: dwa warunki sprzed 3.58.6 i nowa funkcja.
static void komendaAktualizuj() {
  if (czyTemperaturaAlarmowa || alarmPanelWlaczone || czyAlarmAktywny) { sendTelegramMessage(F("Aktywny alarm")); return; }
  if (otaOperacjaWTrakcie) { sendTelegramMessage(F("Inna aktualizacja")); return; }
  panelOtaKlejKomendaAktualizuj();
}
// Oferta wystawiona komenda; zwraca identyfikator pliku.
static std::string wystawOferte() {
  komendaAktualizuj();
  return obrazPliku(atrapa::plikWydania);
}
static const char OBRAZ_PANELU[] = "1111222233334444";   // program, z ktorego panel pracuje przed aktualizacja

// ---------------------------------------------------------------------------
// 1. Narzedzia bloku decyzji
// ---------------------------------------------------------------------------
static void testNarzedzia() {
  printf("[1] identyfikator pliku, sprawdzenie naglowka, czytanie odmowy\n");
  const uint8_t suma[32] = {0x00, 0x1f, 0xa0, 0xff, 0x12, 0x34, 0x56, 0x78, 0x99};
  char o[17];
  panelOtaObrazZSumy(suma, o);
  sprawdz(strcmp(o, "001fa0ff12345678") == 0, "identyfikator = 8 pierwszych bajtow sumy, male litery szesnastkowe");
  sprawdz(panelOtaObrazPoprawny("001fa0ff12345678"), "16 znakow 0-9 a-f: poprawny");
  sprawdz(!panelOtaObrazPoprawny("001FA0FF12345678") && !panelOtaObrazPoprawny("001fa0ff1234567") && !panelOtaObrazPoprawny("001fa0ff123456789") &&
          !panelOtaObrazPoprawny("") && !panelOtaObrazPoprawny(nullptr) && !panelOtaObrazPoprawny("001fa0ff1234567g"),
          "wielkie litery, 15 i 17 znakow, pusty, nullptr, znak spoza zakresu: niepoprawny");

  // skan: ten sam wynik niezaleznie od podzialu na kawalki
  const std::vector<uint8_t> p = programEsp32(5000, 0x21);
  bool jednakowo = true;
  for (size_t kawalek : {1u, 7u, 24u, 31u, 32u, 33u, 512u, 5000u}) {
    PanelOtaSkan s;
    panelOtaSkanStart(s);
    for (size_t i = 0; i < p.size(); i += kawalek) panelOtaSkanDodaj(s, p.data() + i, std::min(kawalek, p.size() - i));
    char ob[17];
    panelOtaSkanObraz(s, ob);
    if (memcmp(s.naglowek, p.data(), 24) != 0 || obrazPliku(p) != ob || s.lacznie != 5000 || panelOtaSkanOcen(s) != nullptr) jednakowo = false;
  }
  sprawdz(jednakowo, "skan pliku: naglowek, koniec i dlugosc takie same przy kawalkach 1, 7, 24, 31, 32, 33, 512 B i w calosci");

  auto ocena = [](const std::vector<uint8_t>& plik) {
    PanelOtaSkan s;
    panelOtaSkanStart(s);
    panelOtaSkanDodaj(s, plik.data(), plik.size());
    const char* w = panelOtaSkanOcen(s);
    return std::string(w ? w : "");
  };
  sprawdz(ocena(programEsp32(5000, 1)).empty(), "program ESP32 z suma na koncu: przyjety");
  sprawdz(zawiera(ocena(programEsp32(5000, 1, 0x0009)), "ESP32-S3"), "program dla ESP32-S3 (firmware centrali): odrzucony z nazwa ukladu");
  sprawdz(zawiera(ocena(programEsp32(5000, 1, 0x0005)), "innego ukladu"), "program dla innego ukladu: odrzucony");
  sprawdz(zawiera(ocena(programEsp32(5000, 1, 0, true, 0x3C)), "zly pierwszy bajt"), "plik, ktory nie zaczyna sie od 0xE9 (np. strona HTML): odrzucony");
  sprawdz(zawiera(ocena(programEsp32(5000, 1, 0, false)), "nie ma sumy"), "program bez sumy SHA-256 na koncu: odrzucony");
  sprawdz(zawiera(ocena(programEsp32(56, 1)), "za krotki"), "plik 56 B (naglowek + suma, bez tresci): odrzucony");
  sprawdz(ocena(programEsp32(57, 1)).empty(), "plik 57 B: najkrotszy przyjmowany");

  uint32_t nr = 0;
  char powod[16];
  sprawdz(panelOtaCzytajOdmowe("12-pobieranie", &nr, powod, sizeof(powod)) && nr == 12 && strcmp(powod, "pobieranie") == 0, "odmowa \"12-pobieranie\": nr 12, powod pobieranie");
  sprawdz(panelOtaCzytajOdmowe("999999999-za_duzy", &nr, powod, sizeof(powod)) && nr == 999999999U, "odmowa z numerem 9-cyfrowym");
  sprawdz(!panelOtaCzytajOdmowe("1234567890-x", &nr, powod, sizeof(powod)), "numer 10-cyfrowy: odrzucony (nie zmiescilby sie pewnie w uint32_t)");
  sprawdz(!panelOtaCzytajOdmowe("-pobieranie", &nr, powod, sizeof(powod)) && !panelOtaCzytajOdmowe("12", &nr, powod, sizeof(powod)) &&
          !panelOtaCzytajOdmowe("12-", &nr, powod, sizeof(powod)) && !panelOtaCzytajOdmowe("12-Pobieranie", &nr, powod, sizeof(powod)) &&
          !panelOtaCzytajOdmowe("12-a<b>", &nr, powod, sizeof(powod)) && !panelOtaCzytajOdmowe("12-abcdefghijklmnop", &nr, powod, sizeof(powod)) &&
          !panelOtaCzytajOdmowe("", &nr, powod, sizeof(powod)) && !panelOtaCzytajOdmowe(nullptr, &nr, powod, sizeof(powod)),
          "odmowa bez numeru, bez powodu, z wielka litera, ze znakami < >, z powodem 16-znakowym, pusta: odrzucona");
  char slowo[16];
  panelOtaCzytajSlowo("bateria", slowo, sizeof(slowo));
  sprawdz(strcmp(slowo, "bateria") == 0, "powod czekania \"bateria\": przyjety");
  panelOtaCzytajSlowo("a b", slowo, sizeof(slowo));
  const bool odrzuconeSpacja = (slowo[0] == '\0');
  panelOtaCzytajSlowo("abcdefghijklmnop", slowo, sizeof(slowo));
  const bool odrzuconeDlugie = (slowo[0] == '\0');
  panelOtaCzytajSlowo("<script>", slowo, sizeof(slowo));
  sprawdz(odrzuconeSpacja && odrzuconeDlugie && slowo[0] == '\0', "powod czekania ze spacja, 16-znakowy, ze znakami < >: pusty");

  char cel[8];
  panelOtaKopiujTekst(cel, sizeof(cel), "v1.4.0-beta");
  sprawdz(strcmp(cel, "v1.4.0-") == 0, "kopia tekstu: obciecie do rozmiaru bufora, zero na koncu");
  char cel2[24];
  panelOtaKopiujTekst(cel2, sizeof(cel2), "a<b>&\"c\\d\x01\xc5\x82");
  sprawdz(strcmp(cel2, "a?b???c?d???") == 0, "kopia tekstu: < > & \" \\ znak sterujacy i znaki spoza ASCII zamienione na ?");
  panelOtaKopiujTekst(cel2, sizeof(cel2), nullptr);
  sprawdz(cel2[0] == '\0', "kopia tekstu: nullptr daje pusty tekst");
}

// ---------------------------------------------------------------------------
// 2. Decyzje bloku (bez kodu klejacego)
// ---------------------------------------------------------------------------
static PanelOtaZgloszenie zgl(const char* wersja, const char* obraz = "", const char* proba = "", bool odmowa = false, uint32_t nr = 0, const char* czeka = "") {
  return PanelOtaZgloszenie{wersja, obraz, proba, odmowa, nr, czeka};
}

static void testDecyzje() {
  printf("[2] decyzje: potwierdzenie, odmowa, starszy panel, limity\n");
  const char* PLIK = "aaaabbbbccccdddd";
  PanelOtaStan s;
  auto oferta = [&](uint32_t teraz = 5000) { panelOtaWystaw(s, 7, "3.58.6", PLIK, 1500000, "v1.4.0", teraz); };

  // --- panel v1.4.0+ ---
  oferta();
  sprawdz(s.aktywna && s.nr == 7 && strcmp(s.tag, "3.58.6") == 0 && strcmp(s.obraz, PLIK) == 0 && s.rozmiar == 1500000 && s.pobranZaczetych == 0 &&
          strcmp(s.wersjaPrzed, "v1.4.0") == 0, "wystawienie oferty: numer, tag, identyfikator, rozmiar, wersja panelu sprzed");
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.0", OBRAZ_PANELU)) == PANEL_OTA_BEZ_ZMIAN && s.aktywna, "panel na innym pliku: oferta czeka");
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.1", "", PLIK)) == PANEL_OTA_BEZ_ZMIAN && s.aktywna && s.probaZgloszona,
          "pierwszy start z nowego pliku (panel_obraz_proba): jeszcze NIE potwierdzenie, oferta czeka");
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.1", PLIK)) == PANEL_OTA_POTWIERDZONO_OBRAZ && !s.aktywna, "panel_obraz rowny identyfikatorowi pliku: potwierdzenie, koniec oferty");
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.1", PLIK)) == PANEL_OTA_BEZ_ZMIAN, "kolejne zgloszenia po koncu oferty: nic");

  oferta();
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.0", OBRAZ_PANELU, "", true, 6)) == PANEL_OTA_BEZ_ZMIAN && s.aktywna, "odmowa z numerem INNEJ oferty: pominieta");
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.0", OBRAZ_PANELU, "", true, 7)) == PANEL_OTA_ODMOWA_PANELU && !s.aktywna, "odmowa z numerem tej oferty: koniec oferty");

  oferta();
  sprawdz(panelOtaPoZgloszeniu(s, zgl("3.58.6", OBRAZ_PANELU)) == PANEL_OTA_BEZ_ZMIAN && s.aktywna,
          "panel v1.4.0+ z wersja rowna tagowi, ale na innym pliku: to NIE potwierdzenie (liczy sie plik)");
  PanelOtaWynik w;
  sprawdz(panelOtaStartPobierania(s, 6000, false, &w) == 200 && s.pobranZaczetych == 1 && s.transferow == 1, "start pobierania: 200, licznik pobran 1");
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.0", OBRAZ_PANELU)) == PANEL_OTA_BEZ_ZMIAN && s.aktywna,
          "panel v1.4.0+ pyta o status W TRAKCIE pobierania (podaje swoj obraz): oferta zostaje");

  // --- panel do v1.3.0 (nie podaje identyfikatora) ---
  oferta();
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.3.0")) == PANEL_OTA_BEZ_ZMIAN && s.aktywna, "starszy panel, jeszcze nie pobieral: oferta czeka");
  sprawdz(panelOtaPoZgloszeniu(s, zgl("3.58.6")) == PANEL_OTA_POTWIERDZONO_WERSJA && !s.aktywna, "starszy panel, wersja rowna tagowi wydania: potwierdzenie (regula sprzed 3.58.6)");
  oferta();
  panelOtaStartPobierania(s, 6000, false, &w);
  panelOtaKoniecPobierania(s, 7);
  sprawdz(panelOtaPoZgloszeniu(s, zgl("v1.4.0")) == PANEL_OTA_STARY_PANEL_WROCIL && !s.aktywna,
          "starszy panel pobral plik i zglasza sie ponownie: koniec oferty (3.58.5: oferta zostawala i panel wgrywal plik przy kazdym starcie)");

  // --- pobieranie ---
  PanelOtaStan pusty = {};
  sprawdz(panelOtaStartPobierania(pusty, 1000, false, &w) == 404 && w == PANEL_OTA_BEZ_ZMIAN, "pobieranie bez oferty: 404");
  oferta();
  sprawdz(panelOtaStartPobierania(s, 6000, true, &w) == 503 && s.pobranZaczetych == 0 && s.aktywna, "pobieranie w czasie alarmu: 503, oferta zostaje, proba sie nie liczy");
  for (int i = 0; i < 4; i++) {
    const int kod = panelOtaStartPobierania(s, 6000 + i, false, &w);
    panelOtaKoniecPobierania(s, 7);
    if (kod != 200) sprawdz(false, "cztery pierwsze pobrania: 200");
  }
  sprawdz(s.pobranZaczetych == 4 && s.aktywna, "po czterech pobraniach oferta jeszcze jest");
  sprawdz(panelOtaStartPobierania(s, 7000, false, &w) == 409 && w == PANEL_OTA_ZA_DUZO_POBRAN && !s.aktywna, "piate pobranie: 409 i koniec oferty");

  // licznik polaczen
  oferta(100000);
  panelOtaStartPobierania(s, 100000, false, &w);
  sprawdz(panelOtaTransferTrwa(s, 100001) && panelOtaTransferTrwa(s, 100000 + PANEL_OTA_TRANSFER_MAX_MS - 1), "plik w drodze: transfer trwa");
  sprawdz(!panelOtaTransferTrwa(s, 100000 + PANEL_OTA_TRANSFER_MAX_MS), "polaczenie starsze niz 5 minut: uznane za zerwane");
  panelOtaKoniecPobierania(s, 8);
  sprawdz(s.transferow == 1, "koniec polaczenia z numerem innej oferty: licznik bez zmian");
  panelOtaStartPobierania(s, 100500, false, &w);
  sprawdz(s.transferow == 2, "drugie polaczenie w tym samym czasie (ponowiona proba): licznik 2");
  panelOtaKoniecPobierania(s, 7);
  sprawdz(panelOtaTransferTrwa(s, 100600), "jedno z dwoch polaczen zamkniete: transfer nadal trwa");
  panelOtaKoniecPobierania(s, 7);
  panelOtaKoniecPobierania(s, 7);
  sprawdz(s.transferow == 0 && !panelOtaTransferTrwa(s, 100700), "oba zamkniete (i jedno zamkniecie za duzo): licznik 0, nie ujemny");
  panelOtaStartPobierania(s, 101000, false, &w);
  sprawdz(panelOtaAnuluj(s) == PANEL_OTA_ANULOWANO && !s.aktywna && panelOtaTransferTrwa(s, 101001),
          "anulowanie w trakcie wysylania: oferta znika, ale plik jest jeszcze w drodze (nie wolno go podmienic)");
  sprawdz(panelOtaAnuluj(s) == PANEL_OTA_BEZ_ZMIAN, "anulowanie bez oferty: nic");

  // --- czas ---
  oferta(5000);
  sprawdz(panelOtaPoCzasie(s, 5000 + PANEL_OTA_WAZNOSC_MS - 1) == PANEL_OTA_BEZ_ZMIAN && s.aktywna, "24 h bez 1 ms: oferta czeka");
  sprawdz(panelOtaPoCzasie(s, 5000 + PANEL_OTA_WAZNOSC_MS) == PANEL_OTA_MINAL_CZAS && !s.aktywna, "po 24 h: koniec oferty");
  oferta(5000);
  panelOtaStartPobierania(s, 5000 + PANEL_OTA_WAZNOSC_MS - 1000, false, &w);
  sprawdz(panelOtaPoCzasie(s, 5000 + PANEL_OTA_WAZNOSC_MS + 1000) == PANEL_OTA_BEZ_ZMIAN && s.aktywna, "limit 24 h mija W TRAKCIE wysylania pliku: oferta zostaje do konca wysylania");
  panelOtaKoniecPobierania(s, 7);
  sprawdz(panelOtaPoCzasie(s, 5000 + PANEL_OTA_WAZNOSC_MS + 2000) == PANEL_OTA_MINAL_CZAS, "...a po zamknieciu polaczenia konczy sie");
  // licznik millis() przekreca sie po 49,7 dnia
  oferta(0xFFFFFF00U);
  sprawdz(panelOtaPoCzasie(s, 0xFFFFFF80U) == PANEL_OTA_BEZ_ZMIAN && s.aktywna, "oferta wystawiona tuz przed przekreceniem licznika czasu: sekunde pozniej nadal czeka");
  sprawdz(panelOtaPoCzasie(s, 0x00000100U) == PANEL_OTA_BEZ_ZMIAN && panelOtaPoCzasie(s, 0xFFFFFF00U + PANEL_OTA_WAZNOSC_MS - 1U) == PANEL_OTA_BEZ_ZMIAN &&
          panelOtaPoCzasie(s, 0xFFFFFF00U + PANEL_OTA_WAZNOSC_MS) == PANEL_OTA_MINAL_CZAS,
          "...po przekreceniu licznika tez, a konczy sie dokladnie po 24 h");

  // --- flaga dla panelu ---
  oferta();
  sprawdz(panelOtaFlagaDlaPanelu(s, false) && !panelOtaFlagaDlaPanelu(s, true), "flaga w /api/status: jest; w czasie alarmu - nie ma");
  panelOtaAnuluj(s);
  sprawdz(!panelOtaFlagaDlaPanelu(s, false), "po koncu oferty flagi nie ma");
}

// ---------------------------------------------------------------------------
// 3. Wiadomosci dla uzytkownika
// ---------------------------------------------------------------------------
static void testWiadomosci() {
  printf("[3] tresc wiadomosci\n");
  PanelOtaStan s;
  panelOtaWystaw(s, 3, "3.58.6", "aaaabbbbccccdddd", 1500000, "v1.3.0", 0);
  char t[480];
  panelOtaKomunikat(PANEL_OTA_POTWIERDZONO_OBRAZ, s, "v1.4.0", "", t, sizeof(t));
  sprawdz(zawiera(t, "Panel zaktualizowany") && zawiera(t, "3.58.6") && zawiera(t, "v1.4.0") && zawiera(t, "przedtem v1.3.0"), "potwierdzenie: wydanie, wersja teraz i przedtem");
  panelOtaKomunikat(PANEL_OTA_POTWIERDZONO_WERSJA, s, "3.58.6", "", t, sizeof(t));
  sprawdz(strcmp(t, "Panel zaktualizowany do 3.58.6.") == 0, "potwierdzenie starszego panelu: tekst jak w 3.58.5");
  for (const char* powod : {"pobieranie", "obraz", "cofnieta", "za_duzy", "haslo", "zapis"}) {
    panelOtaKomunikat(PANEL_OTA_ODMOWA_PANELU, s, "v1.4.0", powod, t, sizeof(t));
    if (!zawiera(t, "Panel nie wgral pliku z wydania 3.58.6") || zawiera(t, "powod podany przez panel jest w logu") || !zawiera(t, "(v1.4.0)")) {
      sprawdz(false, "odmowa: kazdy znany powod ma wlasny opis");
    }
  }
  sprawdz(true, "odmowa: szesc znanych powodow opisanych");
  panelOtaKomunikat(PANEL_OTA_ODMOWA_PANELU, s, "v1.4.0", "cos_nowego", t, sizeof(t));
  sprawdz(zawiera(t, "w logu centrali"), "odmowa z nieznanym powodem: odeslanie do logu");
  panelOtaKomunikat(PANEL_OTA_ODMOWA_PANELU, s, "v1.4.0", "cofnieta", t, sizeof(t));
  sprawdz(zawiera(t, "nie wystartowal") && zawiera(t, "wrocil do poprzedniego"), "odmowa \"cofnieta\": nowy firmware nie wystartowal");
  panelOtaKomunikat(PANEL_OTA_STARY_PANEL_WROCIL, s, "v1.4.0", "", t, sizeof(t));
  sprawdz(zawiera(t, "aktualizacja wykonana") && zawiera(t, "v1.4.0") && zawiera(t, "przedtem v1.3.0"), "starszy panel wrocil z INNA wersja: wykonana, z zastrzezeniem skad to wiadomo");
  panelOtaKomunikat(PANEL_OTA_STARY_PANEL_WROCIL, s, "v1.3.0", "", t, sizeof(t));
  sprawdz(zawiera(t, "nadal zglasza wersje v1.3.0") && zawiera(t, "wycofana"), "starszy panel wrocil z TA SAMA wersja: trzy mozliwe przyczyny, oferta wycofana");
  panelOtaKomunikat(PANEL_OTA_ZA_DUZO_POBRAN, s, "v1.3.0", "", t, sizeof(t));
  sprawdz(zawiera(t, "4 razy"), "za duzo pobran: liczba prob");
  panelOtaKomunikat(PANEL_OTA_MINAL_CZAS, s, "", "", t, sizeof(t));
  sprawdz(zawiera(t, "24 h") && zawiera(t, "/update_panel"), "limit czasu: 24 h i co zrobic dalej");
  panelOtaKomunikat(PANEL_OTA_ANULOWANO, s, "", "", t, sizeof(t));
  sprawdz(zawiera(t, "anulowana"), "anulowanie");
  panelOtaKomunikat(PANEL_OTA_BEZ_ZMIAN, s, "", "", t, sizeof(t));
  sprawdz(t[0] == '\0', "brak decyzji: pusty tekst");
  // za maly bufor: obciecie, bez wyjscia poza bufor
  char maly[40];
  memset(maly, 'X', sizeof(maly));
  panelOtaKomunikat(PANEL_OTA_MINAL_CZAS, s, "", "", maly, 20);
  sprawdz(strlen(maly) == 19 && maly[20] == 'X', "za maly bufor: tekst obciety, nic poza buforem");
  // najdluzsze wiadomosci mieszcza sie w buforze kodu klejacego (480)
  PanelOtaStan dlugi;
  panelOtaWystaw(dlugi, 999999999, "tag-wydania-o-dlugosci-31-znak.", "aaaabbbbccccdddd", 1500000, "wersja-23-znaki-xxxxxxx", 0);
  size_t najdluzsza = 0;
  for (int w = PANEL_OTA_POTWIERDZONO_OBRAZ; w <= PANEL_OTA_ANULOWANO; w++) {
    char duzy[2000];
    panelOtaKomunikat((PanelOtaWynik)w, dlugi, "wersja-23-znaki-yyyyyyy", "cofnieta", duzy, sizeof(duzy));
    najdluzsza = std::max(najdluzsza, strlen(duzy));
    panelOtaKomunikat((PanelOtaWynik)w, dlugi, "wersja-23-znaki-xxxxxxx", "haslo", duzy, sizeof(duzy));
    najdluzsza = std::max(najdluzsza, strlen(duzy));
  }
  PanelOtaPanel p = {};
  for (int wariant = 0; wariant < 4; wariant++) {
    char duzy[2000];
    p.bylKontakt = (wariant > 0);
    p.kontaktMs = 0;
    p.podajeObraz = (wariant > 1);
    strcpy(p.czeka, "pamiec");
    panelOtaTekstPrzypomnienia(dlugi, p, 1000, wariant == 2, "wersja-23-znaki-xxxxxxx", duzy, sizeof(duzy));
    najdluzsza = std::max(najdluzsza, strlen(duzy));
  }
  printf("    najdluzsza wiadomosc: %zu znakow (bufor w kodzie klejacym: 480)\n", najdluzsza);
  sprawdz(najdluzsza < 480, "najdluzsza mozliwa wiadomosc miesci sie w buforze 480 znakow");
}

// ---------------------------------------------------------------------------
// 4. Przebieg: panel v1.4.0, wszystko sie udaje
// ---------------------------------------------------------------------------
static void testPanelNowy() {
  printf("[4] panel v1.4.0: /update_panel -> pobranie -> start z nowego pliku -> potwierdzenie\n");
  nowaCentrala();
  PolaJson j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  sprawdz(!j.dostepna && j.wersja.empty() && j.obraz.empty() && j.nr == 0, "bez oferty: flaga false, pola puste, nr 0");
  sprawdz(zawiera(j.caly, "\"panel_ota_dostepna\":false,\"panel_ota_wersja\":\"\",\"panel_ota_obraz\":\"\",\"panel_ota_nr\":0") && j.caly.back() == '0',
          "pola JSON w ustalonej kolejnosci, bez przecinka na koncu (nastepne pole centrala zaczyna od przecinka)");

  const std::string obraz = wystawOferte();
  sprawdz(atrapa::pobranZGithuba == 1 && atrapa::flagaWTrakciePobierania && !otaOperacjaWTrakcie,
          "otaOperacjaWTrakcie: ustawiona w trakcie pobierania z GitHuba, zdjeta zaraz po (3.58.5: zostawala do potwierdzenia przez panel)");
  sprawdz(zawiera(ostatniTelegram(), "Plik panelu z wydania 3.58.6 jest na centrali (200000 B)") && zawiera(ostatniTelegram(), "/update_panel_anuluj") &&
          !zawiera(ostatniTelegram(), "przycisk"), "wiadomosc po pobraniu: rozmiar, co bedzie dalej, jak anulowac; bez obietnicy \"przy wybudzeniu (przycisk)\"");
  sprawdz(atrapa::nvsBool["panOtaGotowa"] && !atrapa::nvsBool["panOtaTrwa"] && atrapa::nvsTekst["panOtaTag"] == "3.58.6" && atrapa::nvsTekst["panOtaObraz"] == obraz &&
          atrapa::nvsLiczba["panOtaNr"] == 1 && atrapa::nvsLiczba["panOtaRozm"] == 200000 && atrapa::nvsTekst["panOtaPrzed"] == "v1.4.0",
          "NVS: oferta zapisana (znacznik, tag, identyfikator, numer, rozmiar, wersja panelu)");
  sprawdz(logMa("zdarzenie=oferta_wystawiona nr=1 wydanie=3.58.6"), "log: oferta_wystawiona");

  j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  sprawdz(j.dostepna && j.wersja == "3.58.6" && j.obraz == obraz && j.nr == 1, "panel widzi oferte: flaga, tag, identyfikator pliku, numer 1");
  obieg();
  const size_t wiadomosciPrzed = atrapa::telegram.size();

  AsyncWebServerRequest zlyToken = plik("inne-haslo");
  sprawdz(zlyToken.kod == 401 && !zlyToken.wyslanoPlik && g_panelOta.pobranZaczetych == 0, "plik ze zlym haslem: 401, nic sie nie liczy");
  AsyncWebServerRequest bezTokenu = plik(nullptr);
  sprawdz(bezTokenu.kod == 401, "plik bez hasla: 401");

  AsyncWebServerRequest pobranie = plik();
  sprawdz(pobranie.kod == 200 && pobranie.wyslanoPlik && pobranie.tresc == "/panel_firmware.bin" && (bool)pobranie.poRozlaczeniu, "plik z dobrym haslem: 200, wysylany /panel_firmware.bin");
  sprawdz(panelOtaKlejTransferTrwa(), "w trakcie wysylania: transfer trwa (/update centrali odmowi)");
  komendaAktualizuj();
  sprawdz(zawiera(ostatniTelegram(), "Panel wlasnie pobiera plik") && atrapa::pobranZGithuba == 1, "/update_panel w trakcie wysylania pliku: odmowa, plik na dysku nie jest podmieniany");
  // panel pyta o status takze w trakcie pobierania
  j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  sprawdz(j.dostepna, "status w trakcie pobierania: oferta nadal widoczna");
  pobranie.poRozlaczeniu();
  sprawdz(!panelOtaKlejTransferTrwa(), "po rozlaczeniu: transfer skonczony");

  // panel restartuje sie i startuje z nowego pliku
  j = status("v1.4.1", {{"panel_obraz_proba", obraz}});
  sprawdz(j.dostepna && logMa("zdarzenie=panel_startuje_z_nowego_pliku obraz=" ) , "pierwszy start z nowego pliku: wpis w logu, oferta jeszcze widoczna");
  obieg();
  sprawdz(atrapa::telegram.size() == wiadomosciPrzed + 1, "do tej chwili zadnej wiadomosci o wyniku");
  // panel zatwierdzil nowy program
  j = status("v1.4.1", {{"panel_obraz", obraz}});
  sprawdz(!j.dostepna && j.obraz.empty(), "panel_obraz rowny identyfikatorowi pliku: flaga znika w tej samej odpowiedzi");
  sprawdz(atrapa::telegram.size() == wiadomosciPrzed + 1 && atrapa::nvsBool["panOtaGotowa"], "obsluga HTTP niczego nie wysyla i nie pisze do NVS (robi to zadanie Telegrama)");
  obieg();
  sprawdz(zawiera(ostatniTelegram(), "Panel zaktualizowany: pracuje na pliku z wydania 3.58.6 (zglasza wersje v1.4.1, przedtem v1.4.0)"), "wiadomosc: Panel zaktualizowany, z wersja teraz i przedtem");
  sprawdz(!atrapa::nvsBool["panOtaGotowa"] && logMa("zdarzenie=potwierdzono_obraz nr=1 wydanie=3.58.6"), "NVS wyczyszczony, log: potwierdzono_obraz");
  const size_t po = atrapa::telegram.size();
  for (int i = 0; i < 20; i++) { status("v1.4.1", {{"panel_obraz", obraz}}); obieg(); atrapa::terazMs += 2000; }
  sprawdz(atrapa::telegram.size() == po, "kolejne zapytania panelu: zadnej nowej wiadomosci");
  AsyncWebServerRequest poWszystkim = plik();
  sprawdz(poWszystkim.kod == 404, "plik po zakonczonej aktualizacji: 404 (3.58.5: oddawal plik kazdemu z haslem)");
}

// ---------------------------------------------------------------------------
// 5. Przebieg: panel do v1.3.0
// ---------------------------------------------------------------------------
static void testPanelStarszy() {
  printf("[5] panel do v1.3.0: jedna proba, potem koniec oferty\n");
  nowaCentrala();
  status("v1.2.2");
  wystawOferte();
  PolaJson j;
  for (int i = 0; i < 30; i++) { j = status("v1.2.2"); obieg(); atrapa::terazMs += 2000; }
  sprawdz(j.dostepna && j.wersja == "3.58.6", "pracujacy starszy panel pyta co 2 s: oferta czeka (zacznie po wylaczeniu i wlaczeniu)");
  // wylaczenie i wlaczenie panelu: status, potem pobranie (panel w tym czasie o nic nie pyta), restart
  status("v1.2.2");
  AsyncWebServerRequest pobranie = plik();
  sprawdz(pobranie.kod == 200, "starszy panel przy starcie pobiera plik");
  atrapa::terazMs += 40000;
  pobranie.poRozlaczeniu();
  const size_t przed = atrapa::telegram.size();
  // nowy firmware (v1.4.0) startuje: zglasza identyfikator
  j = status("v1.4.0", {{"panel_obraz_proba", obrazPliku(atrapa::plikWydania)}});
  sprawdz(j.dostepna, "nowy firmware przy pierwszym starcie: oferta jeszcze jest");
  j = status("v1.4.0", {{"panel_obraz", obrazPliku(atrapa::plikWydania)}});
  obieg();
  sprawdz(!j.dostepna && atrapa::telegram.size() == przed + 1 && zawiera(ostatniTelegram(), "Panel zaktualizowany: pracuje na pliku") && zawiera(ostatniTelegram(), "przedtem v1.2.2"),
          "starszy panel -> v1.4.0: potwierdzenie po identyfikatorze pliku");

  // wariant: w wydaniu jest znow firmware bez identyfikatora (np. v1.3.0)
  nowaCentrala();
  status("v1.2.2");
  wystawOferte();
  status("v1.2.2");
  AsyncWebServerRequest p2 = plik();
  p2.poRozlaczeniu();
  j = status("v1.3.0");
  sprawdz(!j.dostepna, "starszy firmware po pobraniu zglasza sie ponownie: flaga znika od razu (nie wgra pliku drugi raz)");
  obieg();
  sprawdz(zawiera(ostatniTelegram(), "zglasza teraz wersje v1.3.0 (przedtem v1.2.2) - aktualizacja wykonana"), "wersja sie zmienila: wiadomosc o wykonanej aktualizacji");
  AsyncWebServerRequest p3 = plik();
  sprawdz(p3.kod == 404, "kolejny start starszego panelu: pliku juz nie dostanie (koniec petli z 3.58.5)");

  // wariant: pobieranie sie nie udalo albo plik ma te sama wersje
  nowaCentrala();
  status("v1.2.2");
  wystawOferte();
  AsyncWebServerRequest p4 = plik();
  p4.poRozlaczeniu();
  status("v1.2.2");
  obieg();
  sprawdz(zawiera(ostatniTelegram(), "nadal zglasza wersje v1.2.2") && zawiera(ostatniTelegram(), "wycofana") && logMa("zdarzenie=starszy_panel_wrocil"),
          "ta sama wersja po pobraniu: wiadomosc z mozliwymi przyczynami, oferta wycofana");

  // wariant: regula sprzed 3.58.6 (wersja rowna tagowi)
  nowaCentrala();
  status("v1.2.2");
  wystawOferte();
  status("3.58.6");
  obieg();
  sprawdz(ostatniTelegram() == "Panel zaktualizowany do 3.58.6.", "starszy panel zglasza wersje rowna tagowi: potwierdzenie jak w 3.58.5");
}

// ---------------------------------------------------------------------------
// 6. Odmowa, alarm, limit czasu, anulowanie, przypomnienie
// ---------------------------------------------------------------------------
static void testKonceOferty() {
  printf("[6] odmowa panelu, alarm, limit czasu, anulowanie, przypomnienie\n");
  // --- odmowa ---
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  wystawOferte();
  PolaJson j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}, {"panel_ota_odmowa", "9-pobieranie"}});
  sprawdz(j.dostepna, "odmowa z numerem innej oferty: bez skutku");
  j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}, {"panel_ota_odmowa", "1-<b>"}});
  sprawdz(j.dostepna, "odmowa z niedozwolonymi znakami: bez skutku");
  j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}, {"panel_ota_odmowa", "1-haslo"}});
  obieg();
  sprawdz(!j.dostepna && zawiera(ostatniTelegram(), "Panel nie wgral pliku z wydania 3.58.6: centrala odrzucila haslo OTA panelu") && zawiera(ostatniTelegram(), "(v1.4.0)") &&
          logMa("zdarzenie=odmowa_panelu nr=1") && logMa("powod_panelu=haslo") && !atrapa::nvsBool["panOtaGotowa"],
          "odmowa z numerem tej oferty: koniec oferty, wiadomosc z przyczyna, log, NVS wyczyszczony");

  // --- alarm ---
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  wystawOferte();
  for (int ktory = 0; ktory < 4; ktory++) {
    alarmOgrzewanieWlaczone = (ktory == 0);
    czyTemperaturaAlarmowa = (ktory == 1) ? 1 : 0;
    alarmPanelWlaczone = (ktory == 2);
    czyAlarmAktywny = (ktory == 3) ? 1 : 0;
    j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
    AsyncWebServerRequest p = plik();
    if (j.dostepna || !j.wersja.empty() || !j.obraz.empty() || j.nr != 0 || p.kod != 503 || p.wyslanoPlik) sprawdz(false, "kazdy z czterech alarmow ukrywa oferte i daje 503");
  }
  sprawdz(g_panelOta.aktywna && g_panelOta.pobranZaczetych == 0, "alarm (ogrzewanie / temperatura / panel / dym): oferta ukryta, plik 503, oferta i licznik prob bez zmian");
  const size_t przedKomenda = atrapa::telegram.size();
  komendaAktualizuj();
  sprawdz(atrapa::telegram.size() == przedKomenda + 1 && zawiera(ostatniTelegram(), "Aktywny alarm") && atrapa::pobranZGithuba == 1, "/update_panel w czasie alarmu: odmowa (warunek sprzed 3.58.6)");
  alarmOgrzewanieWlaczone = false; czyTemperaturaAlarmowa = 0; alarmPanelWlaczone = false; czyAlarmAktywny = 0;
  j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  sprawdz(j.dostepna && j.nr == 1, "po alarmie oferta wraca");

  // --- limit czasu ---
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  wystawOferte();
  atrapa::terazMs += 24 * GODZINA - 5000;
  obieg();
  sprawdz(g_panelOta.aktywna, "24 h bez 5 s: oferta czeka");
  atrapa::terazMs += 6000;
  obieg();
  sprawdz(!g_panelOta.aktywna && zawiera(ostatniTelegram(), "w ciagu 24 h") && logMa("zdarzenie=minal_czas") && !atrapa::nvsBool["panOtaGotowa"],
          "po 24 h: koniec oferty, wiadomosc, log, NVS wyczyszczony (3.58.5: oferta bez konca)");

  // --- anulowanie ---
  nowaCentrala();
  panelOtaKlejKomendaAnuluj();
  sprawdz(zawiera(ostatniTelegram(), "Nie ma czego anulowac"), "/update_panel_anuluj bez oferty: informacja");
  wystawOferte();
  panelOtaKlejKomendaAnuluj();
  sprawdz(!g_panelOta.aktywna && zawiera(ostatniTelegram(), "anulowana") && !atrapa::nvsBool["panOtaGotowa"] && !polaJson().dostepna, "/update_panel_anuluj: koniec oferty, NVS wyczyszczony");
  // anulowanie zaraz po potwierdzeniu, zanim zadanie Telegrama je wykonalo: najpierw potwierdzenie
  nowaCentrala();
  const std::string obraz = wystawOferte();
  status("v1.4.1", {{"panel_obraz", obraz}});
  const size_t przedAnulowaniem = atrapa::telegram.size();
  panelOtaKlejKomendaAnuluj();
  sprawdz(atrapa::telegram.size() == przedAnulowaniem + 2 && zawiera(atrapa::telegram[przedAnulowaniem], "Panel zaktualizowany") && zawiera(ostatniTelegram(), "Nie ma czego anulowac"),
          "anulowanie tuz po potwierdzeniu: najpierw wiadomosc o potwierdzeniu, potem \"nie ma czego anulowac\"");

  // --- przypomnienie ---
  nowaCentrala();
  wystawOferte();
  size_t przed = atrapa::telegram.size();
  atrapa::terazMs += 5 * MINUTA - 1000;
  obieg();
  sprawdz(atrapa::telegram.size() == przed, "5 minut bez sekundy: bez przypomnienia");
  atrapa::terazMs += 2000;
  obieg();
  sprawdz(atrapa::telegram.size() == przed + 1 && zawiera(ostatniTelegram(), "panel nie zglasza sie do centrali") && logMa("zdarzenie=panel_nie_zaczal nr=1 kontakt=0"),
          "po 5 minutach, panel nigdy sie nie zglosil: przypomnienie \"panel nie zglasza sie\"");
  atrapa::terazMs += 30 * MINUTA;
  obieg();
  sprawdz(atrapa::telegram.size() == przed + 1 && g_panelOta.aktywna, "przypomnienie tylko raz; oferta czeka dalej");

  nowaCentrala();
  status("v1.2.2");
  wystawOferte();
  for (int i = 0; i < 151; i++) { status("v1.2.2"); atrapa::terazMs += 2000; }
  obieg();
  sprawdz(zawiera(ostatniTelegram(), "Panel ma starszy firmware (v1.2.2)") && zawiera(ostatniTelegram(), "wylacz i wlacz panel"), "starszy panel w kontakcie: przypomnienie \"wylacz i wlacz panel\"");

  nowaCentrala();
  wystawOferte();
  for (int i = 0; i < 151; i++) { status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}, {"panel_ota_czeka", "bateria"}}); atrapa::terazMs += 2000; }
  obieg();
  sprawdz(zawiera(ostatniTelegram(), "Powod podany przez panel: bateria panelu jest slaba") && logMa("czeka=bateria"), "panel v1.4.0 czeka z powodu baterii: przypomnienie z tym powodem");

  nowaCentrala();
  wystawOferte();
  czyAlarmAktywny = 1;
  for (int i = 0; i < 151; i++) { status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}}); atrapa::terazMs += 2000; }
  obieg();
  sprawdz(zawiera(ostatniTelegram(), "trwa alarm") && g_panelOta.aktywna, "alarm przez 5 minut: przypomnienie \"trwa alarm\", oferta czeka");

  nowaCentrala();
  wystawOferte();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  AsyncWebServerRequest p = plik();
  przed = atrapa::telegram.size();
  atrapa::terazMs += 6 * MINUTA;
  obieg();
  sprawdz(atrapa::telegram.size() == przed, "panel zaczal pobierac: przypomnienia nie ma (wynik przyjdzie osobno)");

  // --- za duzo pobran ---
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  wystawOferte();
  for (int i = 0; i < 4; i++) { AsyncWebServerRequest q = plik(); q.poRozlaczeniu(); }
  AsyncWebServerRequest piate = plik();
  obieg();
  sprawdz(piate.kod == 409 && !piate.wyslanoPlik && zawiera(ostatniTelegram(), "4 razy") && !g_panelOta.aktywna, "piate rozpoczete pobranie: 409, koniec oferty, wiadomosc");

  // --- brak pliku na dysku mimo oferty ---
  nowaCentrala();
  wystawOferte();
  atrapa::plikNaDysku = false;
  AsyncWebServerRequest brak = plik();
  sprawdz(brak.kod == 404 && !panelOtaKlejTransferTrwa() && g_panelOta.pobranZaczetych == 1 && logMa("zdarzenie=brak_pliku_dla_panelu"),
          "oferta jest, pliku nie ma: 404, wpis w logu, licznik polaczen wraca do zera");
}

// ---------------------------------------------------------------------------
// 7. /update_panel: przypadki brzegowe
// ---------------------------------------------------------------------------
static void testKomenda() {
  printf("[7] /update_panel: bledy GitHuba, ten sam plik, zastapienie oferty\n");
  nowaCentrala();
  atrapa::githubOk = false;
  komendaAktualizuj();
  sprawdz(zawiera(ostatniTelegram(), "Nie udalo sie sprawdzic wersji: brak internetu") && !g_panelOta.aktywna && !otaOperacjaWTrakcie, "GitHub nie odpowiada: komunikat, bez oferty");

  nowaCentrala();
  atrapa::pobranieOk = false;
  atrapa::pobranieBlad = "w wydaniu 3.58.6 nie ma pliku firmware_panel.bin (kod 404).";
  komendaAktualizuj();
  sprawdz(zawiera(ostatniTelegram(), "Pobieranie firmware panelu nieudane: w wydaniu 3.58.6 nie ma pliku firmware_panel.bin") && !g_panelOta.aktywna && !otaOperacjaWTrakcie &&
          !atrapa::nvsBool["panOtaTrwa"] && !atrapa::nvsBool["panOtaGotowa"], "pobieranie nieudane: komunikat, flaga i znaczniki NVS zdjete, bez oferty");

  nowaCentrala();
  atrapa::plikWydania = programEsp32(200000, 0x40, 0x0009);
  komendaAktualizuj();
  sprawdz(zawiera(ostatniTelegram(), "ESP32-S3") && !g_panelOta.aktywna && !polaJson().dostepna, "w wydaniu jako firmware_panel.bin lezy firmware centrali: odrzucony, bez oferty");

  // panel zglasza wersje rowna tagowi (regula sprzed 3.58.6)
  nowaCentrala();
  status("3.58.6");
  komendaAktualizuj();
  sprawdz(zawiera(ostatniTelegram(), "Panel juz ma najnowsza wersje (3.58.6)") && atrapa::pobranZGithuba == 0, "panel zglasza wersje rowna tagowi: nic do pobrania");

  // panel pracuje juz na tym pliku
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", obrazPliku(atrapa::plikWydania)}});
  komendaAktualizuj();
  sprawdz(zawiera(ostatniTelegram(), "Panel juz pracuje na pliku z wydania 3.58.6 (zglasza wersje v1.4.0)") && !g_panelOta.aktywna && !atrapa::nvsBool["panOtaGotowa"] &&
          logMa("zdarzenie=panel_juz_ma_ten_plik"), "panel pracuje juz na pliku z wydania: bez oferty (inaczej przyszlaby wiadomosc \"zaktualizowany\", choc nic sie nie stalo)");
  // ...ale gdy panel zamilkl ponad minute temu, nie wiemy, na czym pracuje - oferta powstaje
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", obrazPliku(atrapa::plikWydania)}});
  atrapa::terazMs += 2 * MINUTA;
  komendaAktualizuj();
  sprawdz(g_panelOta.aktywna, "panel bez kontaktu od 2 minut: oferta powstaje (potwierdzi sie przy pierwszym zgloszeniu)");
  status("v1.4.0", {{"panel_obraz", obrazPliku(atrapa::plikWydania)}});
  obieg();
  sprawdz(!g_panelOta.aktywna && zawiera(ostatniTelegram(), "Panel zaktualizowany: pracuje na pliku"), "...i konczy sie przy pierwszym zgloszeniu panelu");

  // druga komenda przy czekajacej ofercie: nowa oferta, nowy numer
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  wystawOferte();
  atrapa::plikWydania = programEsp32(210000, 0x55);
  atrapa::githubTag = "3.58.7";
  const std::string obraz2 = wystawOferte();
  PolaJson j = polaJson();
  sprawdz(j.dostepna && j.nr == 2 && j.wersja == "3.58.7" && j.obraz == obraz2 && atrapa::nvsLiczba["panOtaNr"] == 2 && atrapa::nvsLiczba["panOtaRozm"] == 210000,
          "druga komenda przy czekajacej ofercie: nowa oferta nr 2 z nowym plikiem");
  sprawdz(logMa("zdarzenie=oferta_zastapiona nr=1 wydanie=3.58.6"), "log: oferta_zastapiona");
  sprawdz(!atrapa::ofertaWTrakciePobierania,
          "w trakcie pobierania nowego pliku z GitHuba poprzednia oferta jest juz wycofana (panel nie zacznie pobierac pliku, ktory zaraz zniknie)");
  bool wspomniano = false;
  for (const std::string& t : atrapa::telegram) if (zawiera(t, "Poprzednia, niewgrana oferta (wydanie 3.58.6) zostaje zastapiona")) wspomniano = true;
  sprawdz(wspomniano, "wiadomosc mowi, ze poprzednia oferta zostaje zastapiona");
  j = status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}, {"panel_ota_odmowa", "1-pobieranie"}});
  sprawdz(j.dostepna, "spozniona odmowa oferty nr 1 nie konczy oferty nr 2");

  // numer oferty po 999999999 wraca do 1 (panel odsyla najwyzej 9 cyfr)
  nowaCentrala();
  atrapa::nvsLiczba["panOtaNr"] = 999999999U;
  wystawOferte();
  sprawdz(polaJson().nr == 1, "numer oferty po 999999999: 1");

  // tag wydania ze znakami, ktore zepsulyby JSON albo wiadomosc
  nowaCentrala();
  atrapa::githubTag = "v1\"<x>\\";
  wystawOferte();
  j = polaJson();
  sprawdz(j.dostepna && j.wersja == "v1??x??" && zawiera(j.caly, "\"panel_ota_wersja\":\"v1??x??\","), "tag ze znakami \" < > \\: w JSON-ie zamienione na ?");

  // --- panel konczy poprzednia aktualizacje w chwili, gdy /update_panel pyta GitHuba o nastepna ---
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  const std::string pierwszyObraz = wystawOferte();
  atrapa::plikWydania = programEsp32(230000, 0x66);   // w wydaniu jest juz inny plik
  atrapa::githubTag = "3.58.7";
  atrapa::wTrakciePytaniaGithuba = [&]() { status("v1.4.1", {{"panel_obraz", pierwszyObraz}}); };
  size_t odWiadomosci = atrapa::telegram.size();
  komendaAktualizuj();
  size_t nrPotwierdzenia = 0, nrPobieram = 0;
  for (size_t i = odWiadomosci; i < atrapa::telegram.size(); i++) {
    if (zawiera(atrapa::telegram[i], "Panel zaktualizowany: pracuje na pliku z wydania 3.58.6")) nrPotwierdzenia = i;
    if (zawiera(atrapa::telegram[i], "Wydanie 3.58.7: pobieram plik panelu")) nrPobieram = i;
  }
  sprawdz(nrPotwierdzenia > 0 && nrPobieram > nrPotwierdzenia && !zawiera(atrapa::telegram[nrPobieram], "zostaje zastapiona"),
          "wynik poprzedniej oferty przychodzi w trakcie pytania GitHuba: najpierw wiadomosc o nim, potem o nowym pliku");
  sprawdz(g_panelOta.aktywna && g_panelOta.nr == 2 && atrapa::nvsBool["panOtaGotowa"] && atrapa::nvsLiczba["panOtaNr"] == 2 && !g_panelOtaDoWykonania.jest,
          "...a nowa oferta (nr 2) stoi i jest zapisana w NVS");

  // to samo, ale zadanie Telegrama nie dostaje pamieci na wykonanie wyniku przed wystawieniem nowej oferty
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  const std::string obrazA = wystawOferte();
  atrapa::plikWydania = programEsp32(230000, 0x66);
  atrapa::githubTag = "3.58.7";
  atrapa::wTrakciePytaniaGithuba = [&]() { status("v1.4.1", {{"panel_obraz", obrazA}}); atrapa::odmowPamieci = 1; };
  komendaAktualizuj();
  sprawdz(g_panelOta.aktywna && g_panelOta.nr == 2 && g_panelOtaDoWykonania.jest && atrapa::nvsBool["panOtaGotowa"],
          "wynik poprzedniej oferty czeka (sterta odmowila), nowa oferta juz wystawiona");
  odWiadomosci = atrapa::telegram.size();
  obieg();
  sprawdz(atrapa::telegram.size() == odWiadomosci + 1 && zawiera(ostatniTelegram(), "Panel zaktualizowany: pracuje na pliku z wydania 3.58.6") &&
          logMa("zdarzenie=potwierdzono_obraz nr=1 wydanie=3.58.6"), "zalegly wynik oferty nr 1 wykonany pozniej: wiadomosc i wpis w logu");
  sprawdz(atrapa::nvsBool["panOtaGotowa"] && g_panelOta.aktywna && g_panelOta.nr == 2, "...i NIE kasuje znacznika NVS oferty nr 2 (po restarcie centrali wroci)");
  restartCentrali();
  sprawdz(g_panelOta.aktywna && g_panelOta.nr == 2 && polaJson().wersja == "3.58.7", "restart centrali: oferta nr 2 wraca");
}

// ---------------------------------------------------------------------------
// 8. Restart centrali
// ---------------------------------------------------------------------------
static void testRestartCentrali() {
  printf("[8] restart centrali z czekajaca oferta\n");
  nowaCentrala();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  const std::string obraz = wystawOferte();
  AsyncWebServerRequest p = plik();
  p.poRozlaczeniu();
  restartCentrali();
  PolaJson j = polaJson();
  sprawdz(j.dostepna && j.nr == 1 && j.wersja == "3.58.6" && j.obraz == obraz && g_panelOta.rozmiar == 200000 && strcmp(g_panelOta.wersjaPrzed, "v1.4.0") == 0,
          "po restarcie oferta wraca: ten sam numer, tag, identyfikator, rozmiar");
  sprawdz(!otaOperacjaWTrakcie, "po restarcie otaOperacjaWTrakcie = false (3.58.5: true, czyli /update zablokowane i nadzor zadan wylaczony)");
  sprawdz(zawiera(ostatniTelegram(), "oferta wraca na kolejne 24 h") && logMa("zdarzenie=przywrocono_oferte_po_restarcie nr=1"), "wiadomosc i log o przywroconej ofercie");
  status("v1.4.1", {{"panel_obraz", obraz}});
  obieg();
  sprawdz(zawiera(ostatniTelegram(), "Panel zaktualizowany") && !atrapa::nvsBool["panOtaGotowa"], "potwierdzenie po restarcie centrali dziala");
  const size_t ile = atrapa::telegram.size();
  restartCentrali();
  sprawdz(!polaJson().dostepna && atrapa::telegram.size() == ile, "kolejny restart: zakonczona oferta juz nie wraca, bez wiadomosci");

  // plik zniknal z dysku
  nowaCentrala();
  wystawOferte();
  atrapa::plikNaDysku = false;
  restartCentrali();
  sprawdz(!polaJson().dostepna && !atrapa::nvsBool["panOtaGotowa"] && zawiera(ostatniTelegram(), "nie da sie jej wznowic") && logMa("zdarzenie=oferta_nie_do_przywrocenia"),
          "po restarcie pliku nie ma na dysku: oferta nie wraca, wiadomosc z prosba o ponowne /update_panel");

  // zapis z 3.58.5: znacznik i tag, bez identyfikatora pliku
  nowaCentrala();
  atrapa::nvsBool["panOtaGotowa"] = true;
  atrapa::nvsBool["panOtaTrwa"] = true;
  atrapa::nvsTekst["panOtaTag"] = "3.57.9";
  atrapa::plikNaDysku = true;
  restartCentrali();
  sprawdz(!polaJson().dostepna && !atrapa::nvsBool["panOtaGotowa"] && zawiera(ostatniTelegram(), "wydanie 3.57.9") && zawiera(ostatniTelegram(), "/update_panel") && !otaOperacjaWTrakcie,
          "oferta zapisana przez 3.58.5 (bez identyfikatora): nie jest wznawiana, centrala prosi o ponowne /update_panel");

  // zapis niepelny: jest rozmiar i numer, ale identyfikatora pliku brak albo jest uszkodzony
  for (const char* zlyObraz : {"", "xyz", "AAAABBBBCCCCDDDD"}) {
    nowaCentrala();
    atrapa::nvsBool["panOtaGotowa"] = true;
    atrapa::nvsTekst["panOtaTag"] = "3.58.6";
    atrapa::nvsLiczba["panOtaNr"] = 3;
    atrapa::nvsLiczba["panOtaRozm"] = 200000;
    if (zlyObraz[0] != '\0') atrapa::nvsTekst["panOtaObraz"] = zlyObraz;
    atrapa::plikNaDysku = true;
    restartCentrali();
    if (polaJson().dostepna || atrapa::nvsBool["panOtaGotowa"]) sprawdz(false, "zapis bez poprawnego identyfikatora pliku nie jest wznawiany");
  }
  sprawdz(true, "zapis z rozmiarem i numerem, ale bez poprawnego identyfikatora pliku (brak, za krotki, wielkie litery): nie jest wznawiany");
  // zapis bez numeru oferty
  nowaCentrala();
  atrapa::nvsBool["panOtaGotowa"] = true;
  atrapa::nvsTekst["panOtaTag"] = "3.58.6";
  atrapa::nvsTekst["panOtaObraz"] = "aaaabbbbccccdddd";
  atrapa::nvsLiczba["panOtaRozm"] = 200000;
  atrapa::plikNaDysku = true;
  restartCentrali();
  sprawdz(!polaJson().dostepna, "zapis bez numeru oferty: nie jest wznawiany");
  // plik na dysku ma inny rozmiar niz oferowany
  nowaCentrala();
  wystawOferte();
  atrapa::plikWydania = programEsp32(123456, 0x40);
  restartCentrali();
  sprawdz(!polaJson().dostepna && zawiera(ostatniTelegram(), "nie da sie jej wznowic"), "plik na dysku ma inny rozmiar niz oferowany: oferta nie wraca");

  // restart w trakcie pobierania z GitHuba
  nowaCentrala();
  atrapa::nvsBool["panOtaTrwa"] = true;
  atrapa::nvsTekst["panOtaTag"] = "3.58.6";
  restartCentrali();
  sprawdz(!atrapa::nvsBool["panOtaTrwa"] && zawiera(ostatniTelegram(), "nie dokonczyla sie") && logMa("zdarzenie=przerwane_pobieranie_po_restarcie"),
          "restart w trakcie pobierania z GitHuba: znacznik sprzatniety, wiadomosc jak w 3.58.5");

  // nic nie czekalo
  nowaCentrala();
  restartCentrali();
  sprawdz(atrapa::telegram.empty() && atrapa::log.empty() && atrapa::zapisowNvs == 0, "restart bez oferty: zadnej wiadomosci, logu ani zapisu do NVS");
}

// ---------------------------------------------------------------------------
// 9. Pamiec robocza zadania Telegrama: sterta zamiast stosu
// ---------------------------------------------------------------------------
static void testPamiecRobocza() {
  printf("[9] pamiec robocza zadania Telegrama: ze sterty, zwalniana, odmowa sterty niczego nie gubi\n");
  // zwykly stan: nie ma oferty
  nowaCentrala();
  int przed = atrapa::przydzialow;
  for (int i = 0; i < 100; i++) { atrapa::terazMs += 3000; obieg(); }
  sprawdz(atrapa::przydzialow == przed && atrapa::telegram.empty() && atrapa::log.empty(),
          "bez oferty: 100 obiegow zadania Telegrama nie bierze pamieci, nic nie wysyla i nic nie zapisuje");

  // oferta czeka: obieg bierze jeden blok i oddaje go
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  const std::string obraz = wystawOferte();
  przed = atrapa::przydzialow;
  atrapa::terazMs += 3000;
  obieg();
  sprawdz(atrapa::przydzialow == przed + 1 && atrapa::blokowZywych == 0, "oferta czeka: obieg bierze jeden blok pamieci i od razu go zwalnia");
  sprawdz(atrapa::najwiekszyBlok == sizeof(PanelOtaKlejRobocze) && sizeof(PanelOtaKlejRobocze) <= 1024,
          "blok ma rozmiar PanelOtaKlejRobocze, nie wiecej niz 1 kB");

  // wynik z obslugi HTTP, a sterta odmawia
  status("v1.4.1", {{"panel_obraz", obraz}});
  size_t wiadomosci = atrapa::telegram.size();
  size_t wpisow = atrapa::log.size();
  atrapa::odmowPamieci = 1;
  obieg();
  sprawdz(atrapa::telegram.size() == wiadomosci && atrapa::log.size() == wpisow && g_panelOtaDoWykonania.jest && atrapa::nvsBool["panOtaGotowa"],
          "potwierdzenie czeka, sterta odmawia: wynik nie ginie - bez wiadomosci, bez wpisu, NVS nietkniety");
  obieg();
  sprawdz(atrapa::telegram.size() == wiadomosci + 1 && zawiera(ostatniTelegram(), "Panel zaktualizowany") && !atrapa::nvsBool["panOtaGotowa"] &&
          !g_panelOtaDoWykonania.jest, "nastepny obieg, pamiec jest: wiadomosc o wyniku (jedna) i wyczyszczony NVS");

  // limit 24 h, a sterta odmawia
  nowaCentrala();
  wystawOferte();
  status("v1.4.0", {{"panel_obraz", OBRAZ_PANELU}});
  AsyncWebServerRequest zaczete = plik();   // panel zaczal pobierac: przypomnienia nie bedzie
  zaczete.poRozlaczeniu();
  atrapa::terazMs += 24 * GODZINA + 1000;
  wiadomosci = atrapa::telegram.size();
  atrapa::odmowPamieci = 1;
  obieg();
  sprawdz(g_panelOta.aktywna && atrapa::telegram.size() == wiadomosci && atrapa::nvsBool["panOtaGotowa"], "24 h minely, sterta odmawia: oferta jeszcze stoi, nic nie wyslane");
  obieg();
  sprawdz(!g_panelOta.aktywna && atrapa::telegram.size() == wiadomosci + 1 && zawiera(ostatniTelegram(), "w ciagu 24 h") && !atrapa::nvsBool["panOtaGotowa"],
          "nastepny obieg: koniec oferty po 24 h, wiadomosc, NVS wyczyszczony");

  // przypomnienie, a sterta odmawia
  nowaCentrala();
  wystawOferte();
  atrapa::terazMs += 5 * MINUTA + 1000;
  wiadomosci = atrapa::telegram.size();
  atrapa::odmowPamieci = 1;
  obieg();
  sprawdz(atrapa::telegram.size() == wiadomosci && !g_panelOta.przypomniano, "pora na przypomnienie, sterta odmawia: przypomnienie nie przepada");
  obieg();
  obieg();
  sprawdz(atrapa::telegram.size() == wiadomosci + 1 && zawiera(ostatniTelegram(), "panel nie zglasza sie do centrali") && g_panelOta.przypomniano,
          "nastepne obiegi: przypomnienie wyslane raz");

  // anulowanie, a sterta odmawia (dwa razy: obieg na poczatku komendy i sama komenda)
  nowaCentrala();
  wystawOferte();
  wiadomosci = atrapa::telegram.size();
  atrapa::odmowPamieci = 2;
  panelOtaKlejKomendaAnuluj();
  sprawdz(g_panelOta.aktywna && atrapa::nvsBool["panOtaGotowa"] && atrapa::telegram.size() == wiadomosci + 1 && zawiera(ostatniTelegram(), "nic nie zostalo anulowane"),
          "/update_panel_anuluj, sterta odmawia: oferta zostaje, uzytkownik dostaje informacje");
  panelOtaKlejKomendaAnuluj();
  sprawdz(!g_panelOta.aktywna && !atrapa::nvsBool["panOtaGotowa"] && zawiera(ostatniTelegram(), "anulowana"), "/update_panel_anuluj ponownie, pamiec jest: oferta anulowana");

  // /update_panel, a sterta odmawia obiegowi na poczatku komendy: komenda idzie dalej
  nowaCentrala();
  wystawOferte();
  atrapa::plikWydania = programEsp32(210000, 0x55);
  atrapa::odmowPamieci = 1;
  komendaAktualizuj();
  sprawdz(g_panelOta.aktywna && g_panelOta.nr == 2 && g_panelOta.rozmiar == 210000 && zawiera(ostatniTelegram(), "Plik panelu z wydania 3.58.6 jest na centrali (210000 B)"),
          "/update_panel przy odmowie sterty na poczatku komendy: nowa oferta wystawiona jak zwykle");

  // restart centrali z czekajaca oferta: bez pamieci roboczej
  przed = atrapa::przydzialow;
  restartCentrali();
  sprawdz(g_panelOta.aktywna && atrapa::przydzialow == przed && zawiera(ostatniTelegram(), "oferta wraca"), "restart centrali z oferta: oferta wraca, wiadomosc, bez pamieci roboczej");
}

int main() {
  printf("Testy kodu [v3.58.6 PANEL-OTA] centrali (posrednictwo aktualizacji panelu LCD)\n");
  testNarzedzia();
  testDecyzje();
  testWiadomosci();
  testPanelNowy();
  testPanelStarszy();
  testKonceOferty();
  testKomenda();
  testRestartCentrali();
  testPamiecRobocza();
  printf("[10] zasady z calego przebiegu: sekcja krytyczna, pamiec robocza, tekst bledu pobierania\n");
  sprawdz(atrapa::naruszen == 0, "w sekcji krytycznej ani razu nie uzyto String, sterty, logu, NVS, Telegrama ani odpowiedzi HTTP");
  sprawdz(atrapa::sekcja == 0, "kazda sekcja krytyczna zostala zamknieta");
  sprawdz(atrapa::przydzialow > 0 && atrapa::blokowZywych == 0, "kazdy blok pamieci roboczej zostal zwolniony");
  sprawdz(atrapa::pobranZNiepustymBledem == 0, "pobierzFirmwarePanelaZGithub() zawsze dostaje pusty tekst bledu (w centrali poznaje po nim, czy blad juz opisano)");
  printf("%s: %d sprawdzen, %d bledow\n", g_bledow == 0 ? "WSZYSTKO OK" : "SA BLEDY", g_sprawdzen, g_bledow);
  return g_bledow == 0 ? 0 : 1;
}
