// ============================================================================
// symulator_panelu.cpp - PRAWDZIWY firmware panelu (src/main.cpp + dashboard
// LVGL) uruchomiony na PC przeciwko udawanym centralom
// ============================================================================
// Po co: w tym projekcie nie ma jak sprawdzic zmian w panelu bez plytki, a
// blad zgloszony 2026-11-10 ("kafelki nie reaguja na zmiany w centrali")
// dotyczyl wlasnie tego, czego nie widac w kodzie: z KTORA centrala panel
// rozmawia i co robi z odpowiedzia, ktorej nie umie odczytac.
//
// Jak to dziala:
//   - src/main.cpp, dashboard_lvgl_poc.cpp i lvgl_port.cpp sa kompilowane BEZ
//     zmian, z atrapami sprzetu z katalogu mock/ (opis: mock/atrapa.h),
//     prawdziwym LVGL 8.3.11 i prawdziwym ArduinoJson 7;
//   - warstwa sieciowa panelu (siec_nieblokujaca_LCD.h) uzywa prawdziwych
//     gniazd; polaczenia do 192.168.4.1:80 sa przekierowane (linker:
//     --wrap=connect) do serwera HTTP w tym pliku, ktory odpowiada jako ta
//     centrala, z ktorej punktem dostepowym "radio" atrapy jest polaczone;
//   - czas plytki (millis) plynie tylko przez delay() firmware'u, wiec
//     kazdy scenariusz jest powtarzalny, a minuta pracy trwa ulamek sekundy.
//
// Uzycie (zwykle przez ./uruchom_testy.sh):
//   symulator_panelu <scenariusz> [--obserwacje] [--log] [--wyjscie KATALOG]
//     --obserwacje  nie oceniaj (kod wyjscia 0), tylko wypisz co zaobserwowano
//                   - do uruchamiania STARSZEJ wersji firmware'u dla porownania
//     --log         wypisuj na biezaco to, co firmware pisze na Serial
//     --wyjscie     gdzie zapisac log Seriala i zrzuty ekranu (.bmp)
//   symulator_panelu --lista     wypisuje nazwy scenariuszy
//
// CZEGO TO NIE SPRAWDZA: sterownika ekranu i dotyku, prawdziwego radia, stosu
// lwIP ani czasow na plytce. Model WiFi jest opisany w mock/atrapa.cpp.
// ============================================================================
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include <lvgl.h>

#include "Arduino.h"
#include "WiFi.h"

// --- z firmware'u (src/main.cpp) ---
void setup();
void loop();
// v1.2.1: pamiec, ktora na plytce przetrwa ESP.restart() (RTC_NOINIT). Starsze
// zrodla (porownanie przez SRC=...) nie maja ani naglowka, ani zmiennej.
#if __has_include("diagnostyka_panelu.h")
#include "diagnostyka_panelu.h"
#endif
#ifdef DIAG_MA_RATUNEK
extern diag::PamiecAutorestartu rtcAutorestart __attribute__((weak));
#define MA_RATUNEK (&rtcAutorestart != nullptr)
#else
#define MA_RATUNEK false
#endif
// --- z dashboardu (src/dashboard_lvgl_poc.cpp) ---
// Naglowek jest we wszystkich wersjach od v1.1.0. Od v1.3.0 definiuje makro
// LVGL_WYKRESY_PUNKTOW i ma inna postac funkcji mostu ekranu Wykresy
// (WykresyFetchCb, ..._strumien_koniec()) - patrz scenariuszWykresyStaryCsv().
#include "dashboard_lvgl_poc.h"
// w v1.1.0 tej funkcji nie ma:
void lvgl_dashboard_poc_aktualizuj_centrale(const char* znacznik) __attribute__((weak));
void pompaManualWyslij(const char* stanTekst);

// LVGL jest tu budowany z include/lv_conf.h TEGO projektu, a ten od v1.2.2
// kieruje bledy wewnetrzne LVGL do panel_lvgl_assert() z main.cpp. Starsze
// zrodla (SRC=... przy porownywaniu wersji) tej funkcji nie maja - dla nich
// ponizsza, slaba wersja robi to, co LVGL robil wtedy na plytce: staje.
extern "C" __attribute__((weak)) void panel_lvgl_assert(void) {
  fprintf(stderr, "LVGL: blad wewnetrzny (assert) - te zrodla firmware'u nie maja panel_lvgl_assert(); na plytce: while(1) i restart przez watchdog\n");
  abort();
}

// Pomiar pamieci (pomiar_pamieci.sh): funkcje z pomiar_sterty.c. W zwyklych
// testach ich nie ma - pomiar_wstrzymaj jest slabym symbolem (mock/atrapa.h).
#ifdef POMIAR_STERTY
extern "C" {
void pomiar_zrzut(const char* plik, int tryb);
void pomiar_zeruj_szczyty(void);
void pomiar_limit(uint32_t esp_bajtow);
void pomiar_bez_limitu(int o_ile);
void pomiar_lap_awarie(void);
uint32_t pomiar_odmow(void);
uint32_t pomiar_zywe_esp(void);
uint32_t pomiar_zywe_bloki(void);
uint32_t pomiar_szczyt_esp(void);
}
#endif

// ---------------------------------------------------------------------------
// Brak pamieci na zadanie
// ---------------------------------------------------------------------------
// Program jest linkowany z --wrap=malloc,calloc,realloc,free: kazde wywolanie
// tych funkcji z firmware'u, LVGL i ArduinoJson (czyli ze wszystkiego, co na
// plytce korzysta z malloc()) przechodzi przez ponizsze owijki. Scenariusz
// moze kazac im odmawiac - tak jak malloc() na plytce, ktorej skonczyl sie
// RAM (log panelu z 2026-11-12: "json=NoMemory", potem zawieszenie w LVGL).
// Odmowa dotyczy tylko watku firmware'u i tylko czasu, gdy dziala firmware
// (LiczFirmware), nie samego testu. Atrapa String (std::string) przydziela
// przez operator new biblioteki standardowej - jej odmowa nie dotyczy.
extern "C" {
void* __real_malloc(size_t n);
void* __real_calloc(size_t ile, size_t rozmiar);
void* __real_realloc(void* p, size_t n);
void __real_free(void* p);
}

namespace pamiec {
size_t odmawiajOd = 0;          // 0 = nie odmawiaj; inaczej odmow przydzialom o rozmiarze odmawiajOd..odmawiajDo
size_t odmawiajDo = (size_t)-1;
int odmow = 0;                  // ile przydzialow dostalo odmowe
thread_local int wFirmware = 0; // >0: wlasnie dziala kod firmware'u
// Bufory punktow ekranu Wykresy: 8 x malloc(96 punktow x 24 B). Sledzone,
// zeby scenariusz mogl sprawdzic, ze istnieja tylko przy otwartym ekranie.
const size_t BUFOR_WYKRESU_B = 96 * 24;
void* buforyWykresow[32];
int zyweBuforyWykresow() {
  int n = 0;
  for (void* p : buforyWykresow) if (p != nullptr) n++;
  return n;
}
bool odmowic(size_t n) {
  if (odmawiajOd == 0 || wFirmware <= 0 || n < odmawiajOd || n > odmawiajDo) return false;
  odmow++;
  return true;
}
void zapamietaj(void* p, size_t n) {
  if (p == nullptr || n != BUFOR_WYKRESU_B || wFirmware <= 0) return;
  for (void*& q : buforyWykresow) if (q == nullptr) { q = p; return; }
}
void zapomnij(void* p) {
  if (p == nullptr) return;
  for (void*& q : buforyWykresow) if (q == p) { q = nullptr; return; }
}
}  // namespace pamiec

extern "C" void* __wrap_malloc(size_t n) {
  if (pamiec::odmowic(n)) return nullptr;
  void* p = __real_malloc(n);
  pamiec::zapamietaj(p, n);
  return p;
}
extern "C" void* __wrap_calloc(size_t ile, size_t rozmiar) {
  if (pamiec::odmowic(ile * rozmiar)) return nullptr;
  return __real_calloc(ile, rozmiar);
}
extern "C" void* __wrap_realloc(void* p, size_t n) {
  if (n != 0 && pamiec::odmowic(n)) return nullptr;   // stary blok zostaje, jak w prawdziwym realloc()
  return __real_realloc(p, n);
}
extern "C" void __wrap_free(void* p) {
  pamiec::zapomnij(p);
  __real_free(p);
}

#ifdef POMIAR_STERTY
// W 32-bitowym modelu (pomiar_pamieci.sh) biblioteka C++ jest wlinkowana
// statycznie, wiec --wrap=malloc objalby takze jej operator new - a wtedy
// odmowa trafialaby w atrape String (std::string) i konczyla program
// wyjatkiem std::bad_alloc. Prawdziwy String z Arduino przy braku pamieci
// zwraca pusty tekst, nie rzuca wyjatku. Dlatego tu operator new idzie
// prosto do prawdziwego malloc(), tak jak w zwyklych testach (gdzie
// biblioteka C++ jest osobna i owijki jej nie dotycza).
void* operator new(size_t n) {
  // Bez limitu pamieci (pomiar_limit()): patrz t_bez_limitu w pomiar_sterty.c.
  pomiar_bez_limitu(1);
  void* p = __real_malloc(n ? n : 1);
  pomiar_bez_limitu(-1);
  if (p == nullptr) throw std::bad_alloc();
  return p;
}
void* operator new[](size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { __real_free(p); }
void operator delete[](void* p) noexcept { __real_free(p); }
void operator delete(void* p, size_t) noexcept { __real_free(p); }
void operator delete[](void* p, size_t) noexcept { __real_free(p); }
#endif

namespace {

// Zakres, w ktorym dziala kod firmware'u (setup(), loop() i funkcje
// dashboardu wolane z testu): tylko wtedy owijki malloc() moga odmawiac i
// tylko to jest liczone przez pomiar pamieci (pomiar_pamieci.sh).
struct LiczFirmware {
  LiczFirmware() { pamiec::wFirmware++; if (pomiar_wstrzymaj != nullptr) pomiar_wstrzymaj(-1); }
  ~LiczFirmware() { pamiec::wFirmware--; if (pomiar_wstrzymaj != nullptr) pomiar_wstrzymaj(1); }
};

// ===========================================================================
// Udawane centrale
// ===========================================================================
enum Tryb {
  TRYB_OK = 0,   // HTTP 200 + poprawny JSON
  TRYB_NAN,      // HTTP 200, ale jedno pole ma wartosc `nan` (tak Arduino wypisuje float NaN)
  TRYB_HTTP500,  // HTTP 500
  TRYB_PUSTA,    // serwer przyjmuje polaczenie i zamyka je, nie wysylajac nic
  TRYB_UCIETA,   // naglowki + polowa tresci, potem zamkniecie
  TRYB_MILCZY,   // serwer przyjmuje zapytanie i nie odpowiada (timeout po stronie panelu)
  TRYB_ODMAWIA   // nikt nie slucha na porcie (connection refused)
};

// Bity Centrala::symulacja - w kolejnosci pol "symulacja_*" w /api/status
// prawdziwej centrali (wifiZbudujJsonStatus() w main_centrala.cpp 3.58.3).
enum SymCentrali {
  SYM_DYM = 1 << 0, SYM_OGRZ = 1 << 1, SYM_BOJLER = 1 << 2, SYM_PANEL = 1 << 3, SYM_POKOJ = 1 << 4,
  SYM_ZEWN = 1 << 5, SYM_POWROT = 1 << 6, SYM_TROCINY = 1 << 7, SYM_CISNIENIE = 1 << 8, SYM_WILGOTNOSC = 1 << 9
};

// Jak centrala traktuje adresy /api/history... (Centrala::wersjaHistorii).
enum WersjaHistorii {
  HIST_PANEL = 0,    // firmware 3.58.4+: zna GET /api/history/panel (gotowe punkty wykresu)
  HIST_302,          // firmware <= 3.58.3: nieznany adres -> przekierowanie 302 na "/" (onNotFound)
  HIST_PRZEDROSTEK   // bardzo stary firmware: "/api/history/cokolwiek" oddaje krotki CSV z kodem 200
};

struct Centrala {
  bool symulator = false;   // true = dodaje "symulator":true, jak centrala-testowa-c3
  int trybStatus = TRYB_OK;
  int trybUstawienia = TRYB_OK;
  int trybPost = TRYB_OK;
  // wartosci pokazywane na kafelkach
  double tZewn = 8.0, tBojler = 42.0, tOgrz = 55.0, tOgrzSr = 54.0, tPanel = 12.0, tPokoj = 21.5;
  double tTrociny = 48.6, tPowrot = 20.1;
  double wilgotnosc = 45.0, cisnienie = 1013.2;
  int dym = 120, klapa = 40, syberka = 60, godzina = 20, minuta = 5, dzien = 10, miesiac = 11, rok = 2026;
  bool pompa = false, dymWlaczony = true, rtcOk = true;
  bool alarmOgrzewanie = false, alarmPanel = false, dymAlarm = false;
  unsigned symulacja = 0;   // suma SYM_*: czujniki, ktore centrala podaje jako symulowane
  // liczniki zapytan, ktore do tej centrali dotarly
  int nStatus = 0, nUstawienia = 0, nPost = 0, nInne = 0;
  std::string ostatniStatusSciezka;   // adres ostatniego zapytania /api/status (z parametrami panel_fw i reset_powod)
  // To, co z parametrem reset_powod robi firmware centrali 3.58.3 (handler
  // /api/status): przy kazdej ZMIANIE wartosci, o ile nowa to nie "sw" ani
  // "deepsleep", pisze linie "tag=RESET-DIAG ... powod=" i wysyla wiadomosc.
  std::string ostatniPowodRestartu;       // g_panelOstatniPowodRestartu centrali (pusty po jej restarcie)
  std::vector<std::string> resetDiag;     // powody, o ktorych centrala napisala w logu i wyslala wiadomosc
  // Posrednictwo OTA panelu - to, co z parametrem panel_fw robi ten sam
  // handler centrali (main_centrala.cpp, znaczniki "[OTA-PANEL]"): flaga
  // "panel_ota_dostepna" znika TYLKO wtedy, gdy tekst zgloszony przez panel
  // jest identyczny z tagiem wydania, na ktore centrala czeka
  // ("if (g_panelOtaDostepna && zgloszona == g_panelOtaWersja)").
  bool panelOtaDostepna = false;          // g_panelOtaDostepna
  std::string panelOtaWersja;             // g_panelOtaWersja: tag wydania z GitHuba
  std::string panelWersjaZgloszona;       // g_panelWersjaZgloszona
  int panelOtaPotwierdzen = 0;            // ile razy centrala uznala aktualizacje za wykonana
  int nStatusZFlagaOta = 0;               // odpowiedzi /api/status wyslane z "panel_ota_dostepna":true

  // --- historia dla ekranu Wykresy (GET /api/history...) - opis: odpowiedzHistorii() ---
  int wersjaHistorii = HIST_PANEL;
  std::map<std::string, std::string> panelHist;   // tryb (godz/dzien/tydz) -> tresc odpowiedzi /api/history/panel
  int panelHistKod = 200;            // 503 = centrali zabraklo pamieci na odpowiedz
  int wierszyKrotkich = 1440;        // stary CSV /api/history: liczba wierszy (probka co 60 s)
  int wierszyDlugich = 2000;         // stary CSV /api/history/long: liczba wierszy (zdarzenie co 5 min)
  int histDziuraOd = -1, histDziuraIle = 0;   // stary CSV /api/history bez wierszy od..od+ile-1 (centrala wtedy nie pracowala)
  bool histHtml = false;             // true: pod kazdym adresem /api/history... strona HTML (to nie centrala pieca)
  bool histBezDlugosci = false;      // true: odpowiedz bez naglowka Content-Length (koniec = zamkniecie polaczenia)
  bool histCrlf = false;             // true: linie zakonczone \r\n
  long histUrwijPo = -1;             // >= 0: po tylu bajtach TRESCI centrala zamyka polaczenie (naglowek zapowiedzial calosc)
  long histZamilknijPo = -1;         // >= 0: po tylu bajtach TRESCI centrala przestaje wysylac, polaczenie zostaje otwarte
  unsigned long histZwlokaMs = 0;    // tyle czasu plytki mija, zanim centrala wysle pierwszy bajt odpowiedzi
  double histBajtowNaMs = 0;         // szybkosc lacza: bajty na milisekunde czasu plytki; 0 = bez ograniczenia
  // co do centrali dotarlo i jak sie skonczylo
  int nPanelHist = 0;                // zapytania o /api/history/panel (niezaleznie od tego, czy centrala zna ten adres)
  int nHistKrotka = 0, nHistDluga = 0;
  int nHistWyslanych = 0;            // odpowiedzi historii wyslane do konca
  int nHistPorzuconych = 0;          // polaczenia historii zamkniete przez PANEL, zanim centrala skonczyla
  std::string ostatniaPanelHistSciezka;
};

const int LICZBA_CENTRAL = 3;
std::mutex g_mutex;                 // chroni g_centrale i g_kolejkaCentral
Centrala g_centrale[LICZBA_CENTRAL];
std::deque<int> g_kolejkaCentral;   // do ktorej centrali nalezy kolejne przyjete polaczenie
std::atomic<int> g_wLocie{0};       // zapytania wyslane przez panel, na ktore serwer jeszcze nie zareagowal
std::atomic<bool> g_koniec{false};
int g_gniazdoSerwera = -1;
int g_portSerwera = 0;
int g_portZamkniety = 0;            // port, na ktorym nikt nie slucha (TRYB_ODMAWIA)
std::vector<int> g_zaparkowane;     // gniazda trzymane otwarte w TRYB_MILCZY (tylko watek serwera)

std::string fN(double v, int miejsc) {
  char b[32];
  if (std::isnan(v)) return "nan";   // tak Arduino String(float) wypisuje NaN
  snprintf(b, sizeof(b), "%.*f", miejsc, v);
  return b;
}
const char* tn(bool v) { return v ? "true" : "false"; }

// /api/status prawdziwej centrali: te same pola, w tej samej kolejnosci i z
// tym samym formatowaniem liczb, co wifiZbudujJsonStatus() w
// main_centrala.cpp 3.58.3 (temperatury 1 miejsce po przecinku, wilgotnosc 0,
// cisnienie 2). Razem ok. 2 kB - panel sklada to z kilku porcji recv().
// Symulator C3 (c.symulator) dokleja wlasne pola "symulator*".
std::string jsonStatus(const Centrala& c, bool zNan) {
  std::string j = "{";
  j += "\"wifi_mode\":\"AP\",\"ip\":\"192.168.4.1\",\"wifi_ssid\":\"\",\"rssi\":0,";
  j += "\"t_zewn\":" + fN(c.tZewn, 1) + ",\"t_bojler\":" + fN(c.tBojler, 1) + ",\"t_ogrz\":" + fN(c.tOgrz, 1) + ",";
  j += "\"t_ogrz_sr\":" + fN(c.tOgrzSr, 1) + ",\"t_panel\":" + fN(c.tPanel, 1) + ",\"t_pokoj\":" + fN(c.tPokoj, 1) + ",";
  j += "\"t_trociny\":" + fN(c.tTrociny, 1) + ",\"t_powrot\":" + fN(c.tPowrot, 1) + ",";
  j += "\"wilgotnosc\":" + fN(c.wilgotnosc, 0) + ",\"hist_fs\":1,";
  j += "\"cisnienie\":" + (zNan ? std::string("nan") : fN(c.cisnienie, 2)) + ",";
  j += "\"dym\":" + std::to_string(c.dym) + ",\"dym_swiezy\":" + tn(c.dymWlaczony) + ",\"dym_wlaczony\":" + tn(c.dymWlaczony) + ",\"dym_alarm\":" + tn(c.dymAlarm) + ",";
  j += "\"alarm_dym_wyciszony\":false,\"alarm_dym_wyciszenie_ms\":0,\"alarm_ogrz_wyciszony\":false,\"alarm_ogrz_wyciszenie_ms\":0,";
  j += "\"alarm_sound_dym\":\"syrena\",\"alarm_sound_ogrz\":\"pulsujacy\",\"alarm_sound_both\":\"syrena\",";
  j += "\"klapa\":" + std::to_string(c.klapa) + ",\"syberka\":" + std::to_string(c.syberka) + ",\"tryb_serwa\":1,";
  j += std::string("\"pompa\":") + tn(c.pompa) + ",\"pompa_ms_od_zmiany\":1049000,\"pompa_override_min\":0,";
  j += "\"wybor\":1,\"mieszadlo\":false,\"rozpalanie\":false,\"rozpalanie_potwierdzone\":false,\"rozpalanie_pozostalo_s\":0,";
  j += std::string("\"alarm_ogrzewanie\":") + tn(c.alarmOgrzewanie) + ",\"alarm_panel\":" + tn(c.alarmPanel) + ",";
  j += "\"rozpalanie_serwa_dostepne\":true,\"rozpalanie_serwa_aktywne\":false,\"rozpalanie_serwa_blokada_dymu\":false,";
  j += "\"rozpalanie_serwa_temp_ok\":true,\"rozpalanie_serwa_alarm_bezpieczenstwa\":false,";
  j += "\"miesiac\":" + std::to_string(c.rtcOk ? c.miesiac : 0) + ",\"godzina\":" + std::to_string(c.rtcOk ? c.godzina : 0);
  j += ",\"minuta\":" + std::to_string(c.rtcOk ? c.minuta : 0) + ",\"dzien\":" + std::to_string(c.rtcOk ? c.dzien : 0);
  j += ",\"rok\":" + std::to_string(c.rtcOk ? c.rok : 0) + ",\"rtc_ok\":" + tn(c.rtcOk) + ",\"internet\":false,";
  j += std::string("\"panel_ota_dostepna\":") + tn(c.panelOtaDostepna) + ",\"panel_ota_wersja\":\"" + c.panelOtaWersja + "\"";
  static const char* const NAZWY_SYM[] = {"dym", "ogrz", "bojler", "panel", "pokoj", "zewn", "ogrz_powrot", "ogrz_trociny", "cisnienie", "wilgotnosc"};
  for (int i = 0; i < 10; i++) {
    const bool aktywna = (c.symulacja & (1u << i)) != 0;
    j += std::string(",\"symulacja_") + NAZWY_SYM[i] + "\":" + tn(aktywna);
    j += std::string(",\"symulacja_") + NAZWY_SYM[i] + "_min\":" + (aktywna ? "60" : "0");
  }
  j += ",\"serwo_override_min\":0,\"mieszadlo_override_min\":0,\"deszcz_stan\":0,\"deszcz_min\":-1";
  if (c.symulator) {
    j += ",\"symulator\":true,\"symulator_panel_fw\":\"?\",\"symulator_stacje\":1,";
    j += "\"symulator_heap\":200000,\"symulator_uptime_s\":1234";
  }
  j += "}";
  return j;
}

// /api/ustawienia-get: te same pola co handler w main_centrala.cpp 3.58.3.
std::string jsonUstawienia() {
  return "{\"wybor\":1,\"trybSerwa\":1,\"czasOn\":5,\"czasOff\":10,\"tempOn\":45,\"tempOff\":40,"
         "\"antystopWlaczony\":true,\"antystopDni\":7,\"mieszadloWlaczony\":false,\"mieszadloCzasOn\":10,"
         "\"mieszadloCzasOff\":60,\"rozpalanie\":false,\"rozpalanie_potwierdzone\":false,\"rozpalanie_pozostalo_s\":0,"
         "\"tempZadServo\":60,\"histerServo\":3,\"skokKlapy\":5,"
         "\"skokSyberka\":5,\"odchylTemp\":2,\"mnoznik\":3,\"progAlarmTemp\":90,\"progAlarmDym\":500,"
         "\"dymCzasOn\":30,\"dymCzasOff\":300,\"dymCzasStabilizacji\":20,\"dymProgTemp\":35,\"dymTrybPracy\":0}";
}

void zapiszWszystko(int fd, const std::string& s) {
  size_t poszlo = 0;
  while (poszlo < s.size()) {
    ssize_t n = write(fd, s.data() + poszlo, s.size() - poszlo);
    if (n <= 0) return;
    poszlo += (size_t)n;
  }
}

// ---------------------------------------------------------------------------
// Historia dla ekranu Wykresy
// ---------------------------------------------------------------------------
// Koniec danych we wszystkich odpowiedziach historii: 2026-11-13 12:00:00 UTC
// = 13:00 czasu polskiego. Ta sama chwila co T0 w testach centrali
// (firmware-patch/v3.58.5-panel-wykresy-po-restarcie/tests/test_panel_hist.cpp).
const uint32_t HIST_KONIEC_UNIX = 1794571200U;

// Odpowiedzi centrali na GET /api/history/panel to PLIKI zapisane kodem
// centrali 3.58.5 (katalog wzorce_centrali/, opis w CZYTAJ.txt obok nich);
// 9 z 11 jest bajt w bajt takich samych, jakie wypisuje centrala 3.58.4.
std::string wzorzec(const char* nazwa) {
  const char* katalog = getenv("LCD_WZORCE_CENTRALI");
  const std::string sciezka = std::string(katalog != nullptr ? katalog : "wzorce_centrali") + "/" + nazwa;
  FILE* f = fopen(sciezka.c_str(), "rb");
  if (f == nullptr) {
    fprintf(stderr, "brak pliku wzorca %s (zmienna LCD_WZORCE_CENTRALI ustawia katalog; robi to uruchom_testy.sh)\n", sciezka.c_str());
    exit(2);
  }
  std::string s;
  char b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
  fclose(f);
  return s;
}

// Czas polski zimowy z czasu Unix - bez zaleznosci od strefy tego komputera.
struct tm czasPolski(uint32_t czasUnix) {
  const time_t t = (time_t)czasUnix + 3600;
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm;
}

// ZEWN w wierszu `i` z `n`: rowno od -5,0 do 9,4 (to samo co we wzorcu dzien.txt).
double zewnWiersza(int i, int n) { return (n > 1) ? -5.0 + 14.4 * i / (n - 1) : -5.0; }

// Stary CSV centrali <= 3.58.3, GET /api/history - format historiaLiniaCsv()
// z main_centrala.cpp: etykieta, 7 temperatur, pompa, cisnienie, wilgotnosc,
// klapa, syberek, czas Unix. Probka co 60 s.
std::string csvKrotki(int wierszy, int dziuraOd, int dziuraIle) {
  std::string s;
  s.reserve((size_t)wierszy * 72);
  for (int i = 0; i < wierszy; i++) {
    if (dziuraOd >= 0 && i >= dziuraOd && i < dziuraOd + dziuraIle) continue;   // centrala wtedy nie pracowala
    const uint32_t czas = HIST_KONIEC_UNIX - (uint32_t)(wierszy - 1 - i) * 60U;
    const struct tm tm = czasPolski(czas);
    char b[200];
    snprintf(b, sizeof(b), "%02d:%02d,%.1f,45.0,%s,55.0,30.0,12.0,21.5,%d,1013.2,45,40,60,%lu\n", tm.tm_hour, tm.tm_min, zewnWiersza(i, wierszy),
             i < wierszy / 2 ? "60.0" : "40.0", i % 2, (unsigned long)czas);
    s += b;
  }
  return s;
}

// Stary CSV centrali <= 3.58.3, GET /api/history/long - format
// historiaDlugaLiniaCsv(): etykieta, 11 x (srednia, min, max), czas Unix,
// pompa. Zdarzenie co 5 minut.
std::string csvDlugi(int wierszy) {
  std::string s;
  s.reserve((size_t)wierszy * 230);
  for (int i = 0; i < wierszy; i++) {
    const uint32_t czas = HIST_KONIEC_UNIX - (uint32_t)(wierszy - 1 - i) * 300U;
    const struct tm tm = czasPolski(czas);
    const double z = zewnWiersza(i, wierszy);
    char b[400];
    snprintf(b, sizeof(b),
             "%02d.%02d %02d:%02d,%.1f,%.1f,%.1f,45.0,44.5,45.5,60.0,59.0,61.0,55.0,54.0,56.0,30.0,29.0,31.0,12.0,11.5,12.5,21.5,21.0,22.0,"
             "1013.2,1013.0,1013.4,45,44,46,40,40,40,60,60,60,%lu,%d\n",
             tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min, z, z - 0.5, z + 0.5, (unsigned long)czas, i % 2);
    s += b;
  }
  return s;
}

// Odpowiedz historii wysylana po kawalku, w rytmie zegara plytki (hakCzasu()).
// Prawdziwa centrala wysyla 20 kB..1,4 MB przez WiFi w czasie, w ktorym petla
// panelu obraca sie wiele razy; tutaj tempo (i ewentualne urwanie albo
// zamilkniecie w polowie) ustawia scenariusz. Wysylanie idzie z watku
// firmware'u, w hakCzasu() - dzieki temu kazdy przebieg jest taki sam.
struct Strumien {
  int fd = -1;
  int centrala = 0;
  std::string dane;                // naglowki HTTP + tresc
  size_t poszlo = 0;
  size_t granica = (size_t)-1;     // tyle bajtow `dane` wolno wyslac (urwanie / zamilkniecie)
  bool zamknijNaGranicy = false;   // true: po granicy zamknij polaczenie; false: milcz, polaczenie otwarte
  unsigned long zwlokaMs = 0;
  double bajtowNaMs = 0;
  double kredyt = 0;
};
std::vector<Strumien> g_strumienie;   // chronione g_mutex

std::string parametrAdresu(const std::string& sciezka, const char* nazwa) {
  const std::string klucz = std::string(nazwa) + "=";
  size_t p = sciezka.find('?');
  while (p != std::string::npos) {
    p++;
    if (sciezka.compare(p, klucz.size(), klucz) == 0) {
      const size_t od = p + klucz.size();
      return sciezka.substr(od, sciezka.find('&', od) - od);
    }
    p = sciezka.find('&', p);
  }
  return "";
}

// Strona, ktora nie jest odpowiedzia centrali pieca (np. portal logowania
// obcej sieci). Ma jedna linie z kilkunastoma przecinkami - tyle, ile trzeba,
// zeby kod czytajacy CSV znalazl w niej "kolumny".
std::string stronaHtml() {
  return "<!DOCTYPE html>\n<html><head><title>Logowanie do sieci</title>\n"
         "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">\n</head>\n<body>\n<p>" +
         std::string(700, 'x') + "</p>\n<p>Podaj haslo, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15</p>\n</body></html>\n";
}

// Co centrala odpowiada na GET /api/history... (wywolywane pod g_mutex).
//   3.58.4:    /api/history/panel -> gotowe punkty; /api/history i /long -> stare CSV (strona WWW ich uzywa)
//   <= 3.58.3: /api/history/panel -> 302 na "/" (onNotFound -> request->redirect("/"))
//   prastara:  /api/history/cokolwiek -> krotki CSV (dopasowanie po przedrostku)
// Naglowki jak ESPAsyncWebServer dla zapytania HTTP/1.0.
void odpowiedzHistorii(Centrala& c, int idx, const std::string& sciezka, Strumien& st) {
  const std::string adres = sciezka.substr(0, sciezka.find('?'));
  int kod = 200;
  std::string cialo;
  const char* typ = "text/plain";
  if (c.histHtml) {
    if (adres == "/api/history/panel") c.nPanelHist++;
    else if (adres == "/api/history/long") c.nHistDluga++;
    else c.nHistKrotka++;
    typ = "text/html";
    cialo = stronaHtml();
  } else if (adres == "/api/history") {
    c.nHistKrotka++;
    cialo = csvKrotki(c.wierszyKrotkich, c.histDziuraOd, c.histDziuraIle);
  } else if (adres == "/api/history/long") {
    c.nHistDluga++;
    cialo = csvDlugi(c.wierszyDlugich);
  } else if (adres == "/api/history/panel") {
    c.nPanelHist++;
    c.ostatniaPanelHistSciezka = sciezka;
    if (c.wersjaHistorii == HIST_PANEL) {
      const std::string tryb = parametrAdresu(sciezka, "tryb");
      kod = c.panelHistKod;
      if (kod == 503) cialo = "brak pamieci\n";
      else if (c.panelHist.count(tryb) != 0) cialo = c.panelHist[tryb];
      else { kod = 400; cialo = "tryb=godz|dzien|tydz\n"; }
    } else if (c.wersjaHistorii == HIST_PRZEDROSTEK) {
      cialo = csvKrotki(c.wierszyKrotkich, c.histDziuraOd, c.histDziuraIle);
    } else {
      kod = 302;
    }
  } else {
    c.nInne++;
    kod = 302;
  }
  if (c.histCrlf) {
    std::string z;
    z.reserve(cialo.size() + cialo.size() / 50);
    for (char znak : cialo) { if (znak == '\n') z += '\r'; z += znak; }
    cialo.swap(z);
  }

  char naglowki[320];
  if (kod == 302) {
    cialo.clear();
    snprintf(naglowki, sizeof(naglowki), "HTTP/1.0 302 Found\r\nContent-Length: 0\r\nLocation: /\r\nConnection: close\r\n\r\n");
  } else if (c.histBezDlugosci) {
    snprintf(naglowki, sizeof(naglowki), "HTTP/1.0 %d %s\r\nContent-Type: %s\r\nConnection: close\r\n\r\n", kod,
             kod == 200 ? "OK" : (kod == 400 ? "Bad Request" : "Service Unavailable"), typ);
  } else {
    snprintf(naglowki, sizeof(naglowki), "HTTP/1.0 %d %s\r\nContent-Length: %zu\r\nContent-Type: %s\r\nConnection: close\r\n\r\n", kod,
             kod == 200 ? "OK" : (kod == 400 ? "Bad Request" : "Service Unavailable"), cialo.size(), typ);
  }
  const size_t dlNaglowkow = strlen(naglowki);
  st.centrala = idx;
  st.dane = std::string(naglowki) + cialo;
  st.zwlokaMs = c.histZwlokaMs;
  st.bajtowNaMs = c.histBajtowNaMs;
  if (kod == 200 && c.histUrwijPo >= 0 && (size_t)c.histUrwijPo < cialo.size()) {
    st.granica = dlNaglowkow + (size_t)c.histUrwijPo;
    st.zamknijNaGranicy = true;
  } else if (kod == 200 && c.histZamilknijPo >= 0 && (size_t)c.histZamilknijPo < cialo.size()) {
    st.granica = dlNaglowkow + (size_t)c.histZamilknijPo;
    st.zamknijNaGranicy = false;
  }
}

// Wysyla kolejne kawalki odpowiedzi historii; `ms` = ile czasu plytki wlasnie
// uplynelo. Wolane z hakCzasu(), czyli z watku firmware'u w trakcie jego
// delay() - ale to praca "centrali", wiec poza pomiarem pamieci firmware'u.
void pompujStrumienie(unsigned long ms) {
  std::lock_guard<std::mutex> blokada(g_mutex);
  for (size_t i = 0; i < g_strumienie.size();) {
    Strumien& s = g_strumienie[i];
    Centrala& c = g_centrale[s.centrala];
    bool koniec = false;
    // Panel zamknal polaczenie (wyjscie z ekranu, porzucone pobieranie)?
    char bajt;
    const ssize_t r = recv(s.fd, &bajt, 1, MSG_PEEK | MSG_DONTWAIT);
    if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
      c.nHistPorzuconych++;
      koniec = true;
    }
    if (!koniec && s.zwlokaMs > 0) s.zwlokaMs = (ms >= s.zwlokaMs) ? 0 : s.zwlokaMs - ms;
    if (!koniec && s.zwlokaMs == 0) {
      const size_t cel = s.granica < s.dane.size() ? s.granica : s.dane.size();
      size_t wolno = cel - s.poszlo;
      if (s.bajtowNaMs > 0) {
        s.kredyt += s.bajtowNaMs * (double)ms;
        if (s.kredyt > 65536.0) s.kredyt = 65536.0;   // lacze nie "nadrabia" przestojow jednym rzutem
        if ((double)wolno > s.kredyt) wolno = (size_t)s.kredyt;
      }
      while (wolno > 0) {
        const ssize_t n = write(s.fd, s.dane.data() + s.poszlo, wolno);
        if (n > 0) {
          s.poszlo += (size_t)n;
          wolno -= (size_t)n;
          if (s.bajtowNaMs > 0) s.kredyt -= (double)n;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
          break;   // bufor gniazda pelny: panel czyta wolniej, niz centrala wysyla
        } else {
          c.nHistPorzuconych++;
          koniec = true;
          break;
        }
      }
      if (!koniec && s.poszlo >= s.dane.size()) {
        c.nHistWyslanych++;
        koniec = true;
      } else if (!koniec && s.poszlo >= s.granica && s.zamknijNaGranicy) {
        koniec = true;   // urwana: zamkniecie przed koncem zapowiedzianej tresci
      }
    }
    if (koniec) {
      close(s.fd);
      g_strumienie.erase(g_strumienie.begin() + (long)i);
    } else {
      i++;
    }
  }
}

void obsluzPolaczenie(int k, int idx) {
  // Zapytanie: naglowki do pustej linii (+ ewentualne cialo POST, ktore
  // panel wysyla w tym samym send() co naglowki).
  std::string zad;
  char buf[2048];
  while (zad.find("\r\n\r\n") == std::string::npos) {
    ssize_t n = read(k, buf, sizeof(buf));
    if (n <= 0) break;   // panel zamknal / limit czasu odczytu
    zad.append(buf, (size_t)n);
  }
  if (zad.empty()) { close(k); return; }   // nic nie przyszlo - to polaczenie nie bylo liczone w g_wLocie

  const bool post = zad.rfind("POST ", 0) == 0;
  const size_t a = zad.find(' ');
  const size_t b = (a == std::string::npos) ? a : zad.find(' ', a + 1);
  const std::string sciezka = (b == std::string::npos) ? "" : zad.substr(a + 1, b - a - 1);

  if (!post && sciezka.rfind("/api/history", 0) == 0) {
    // Historia: odpowiedz bywa duza i idzie po kawalku - patrz Strumien.
    const int flagi = fcntl(k, F_GETFL, 0);
    fcntl(k, F_SETFL, flagi | O_NONBLOCK);
    {
      std::lock_guard<std::mutex> blokada(g_mutex);
      Strumien st;
      st.fd = k;
      odpowiedzHistorii(g_centrale[idx], idx, sciezka, st);
      g_strumienie.push_back(std::move(st));
    }
    g_wLocie--;
    return;
  }

  int tryb = TRYB_OK;
  int kod = 200;
  std::string cialo;
  {
    std::lock_guard<std::mutex> blokada(g_mutex);
    Centrala& c = g_centrale[idx];
    if (post) {
      c.nPost++;
      tryb = c.trybPost;
      cialo = "{\"ok\":true}";
    } else if (sciezka.rfind("/api/status", 0) == 0) {
      c.nStatus++;
      c.ostatniStatusSciezka = sciezka;
      {
        static const char KLUCZ[] = "reset_powod=";
        const size_t p = sciezka.find(KLUCZ);
        if (p != std::string::npos) {
          const size_t od = p + sizeof(KLUCZ) - 1;
          const std::string powod = sciezka.substr(od, sciezka.find('&', od) - od);
          if (powod != c.ostatniPowodRestartu) {
            if (powod != "deepsleep" && powod != "sw") c.resetDiag.push_back(powod);
            c.ostatniPowodRestartu = powod;
          }
        }
      }
      {
        static const char KLUCZ_FW[] = "panel_fw=";
        const size_t p = sciezka.find(KLUCZ_FW);
        if (p != std::string::npos) {
          const size_t od = p + sizeof(KLUCZ_FW) - 1;
          const std::string zgloszona = sciezka.substr(od, sciezka.find('&', od) - od);
          c.panelWersjaZgloszona = zgloszona;
          if (c.panelOtaDostepna && zgloszona == c.panelOtaWersja) {   // jak centrala: rownosc tekstow
            c.panelOtaDostepna = false;
            c.panelOtaPotwierdzen++;
          }
        }
      }
      tryb = c.trybStatus;
      cialo = jsonStatus(c, tryb == TRYB_NAN);
      if (c.panelOtaDostepna && tryb == TRYB_OK) c.nStatusZFlagaOta++;
    } else if (sciezka.rfind("/api/ustawienia-get", 0) == 0) {
      c.nUstawienia++;
      tryb = c.trybUstawienia;
      cialo = jsonUstawienia();
    } else {
      c.nInne++;
      kod = 404;
      cialo = "Not found";
    }
  }

  if (tryb == TRYB_MILCZY) {
    g_zaparkowane.push_back(k);   // trzymaj otwarte, nie odpowiadaj
    if (g_zaparkowane.size() > 64) { close(g_zaparkowane.front()); g_zaparkowane.erase(g_zaparkowane.begin()); }
    g_wLocie--;
    return;
  }
  if (tryb == TRYB_PUSTA) {
    close(k);
    g_wLocie--;
    return;
  }
  if (tryb == TRYB_HTTP500) {
    kod = 500;
    cialo = "Internal Server Error";
  }
  // Naglowki jak ESPAsyncWebServer centrali w odpowiedzi na zapytanie HTTP/1.0
  // panelu (wersja z zapytania wraca w linii statusu).
  char naglowki[256];
  snprintf(naglowki, sizeof(naglowki),
           "HTTP/1.0 %d %s\r\nCache-Control: no-store, max-age=0\r\nConnection: close\r\nContent-Length: %zu\r\nContent-Type: %s\r\n\r\n",
           kod, kod == 200 ? "OK" : (kod == 404 ? "Not Found" : "Internal Server Error"),
           cialo.size(), kod == 200 ? "application/json" : "text/plain");
  if (tryb == TRYB_UCIETA) cialo.resize(cialo.size() / 2);
  zapiszWszystko(k, std::string(naglowki) + cialo);
  close(k);
  g_wLocie--;
}

// Zapytanie wyslane "martwym" polaczeniem WiFi (patrz AtrapaAp::wcielenie):
// centrala go nie dostaje, panel nie dostaje odpowiedzi. Na plytce taka proba
// konczy sie juz na etapie laczenia ("timeout laczenia"); tutaj polaczenie
// TCP powstaje, a brakuje dopiero odpowiedzi ("timeout calkowity") - dla
// panelu jedno i drugie znaczy to samo: centrala milczy.
void obsluzMartwe(int k) {
  char buf[2048];
  const ssize_t n = read(k, buf, sizeof(buf));
  g_zaparkowane.push_back(k);
  if (g_zaparkowane.size() > 64) { close(g_zaparkowane.front()); g_zaparkowane.erase(g_zaparkowane.begin()); }
  if (n > 0) g_wLocie--;
}

void watekSerwera() {
  if (pomiar_wstrzymaj != nullptr) pomiar_wstrzymaj(1 << 20);   // ten watek to "centrala", nie firmware panelu
  while (!g_koniec.load()) {
    int k = accept(g_gniazdoSerwera, nullptr, nullptr);
    if (k < 0) continue;   // limit czasu accept() - sprawdz g_koniec
    int idx = 0;
    {
      std::lock_guard<std::mutex> blokada(g_mutex);
      if (!g_kolejkaCentral.empty()) { idx = g_kolejkaCentral.front(); g_kolejkaCentral.pop_front(); }
    }
    struct timeval limit = {2, 0};
    setsockopt(k, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    if (idx < 0) obsluzMartwe(k);
    else obsluzPolaczenie(k, idx);
  }
}

int otworzNasluch(int* port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in adres;
  memset(&adres, 0, sizeof(adres));
  adres.sin_family = AF_INET;
  adres.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  adres.sin_port = 0;   // system wybiera wolny port
  if (bind(s, (struct sockaddr*)&adres, sizeof(adres)) != 0) { perror("bind"); exit(2); }
  socklen_t len = sizeof(adres);
  getsockname(s, (struct sockaddr*)&adres, &len);
  *port = ntohs(adres.sin_port);
  return s;
}

void uruchomSerwer() {
  signal(SIGPIPE, SIG_IGN);
  // Port "zamkniety": przydzielony i od razu zwolniony - nikt na nim nie slucha.
  int tymczasowe = otworzNasluch(&g_portZamkniety);
  close(tymczasowe);
  g_gniazdoSerwera = otworzNasluch(&g_portSerwera);
  listen(g_gniazdoSerwera, 16);
  struct timeval limit = {0, 100000};
  setsockopt(g_gniazdoSerwera, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
  std::thread(watekSerwera).detach();
}

// Wolane z delay() firmware'u: daj serwerowi (prawdziwy watek, prawdziwe
// gniazda) dokonczyc obsluge zapytan, ktore panel juz wyslal. Dzieki temu
// odpowiedz jest gotowa po tej samej liczbie obrotow loop() w kazdym
// uruchomieniu - scenariusze sa powtarzalne.
void hakCzasu(unsigned long ms) {
  for (int i = 0; i < 4000 && g_wLocie.load() > 0; i++) usleep(50);   // najwyzej 0,2 s prawdziwego czasu
  // Reszta to praca "centrali" wykonywana w watku firmware'u: bez odmow
  // malloc() (scenariusze braku pamieci) i poza pomiarem pamieci firmware'u.
  const int bylo = pamiec::wFirmware;
  pamiec::wFirmware = 0;
  {
    AtrapaBezPomiaru bezPomiaru;
    pompujStrumienie(ms);
  }
  pamiec::wFirmware = bylo;
}

// ===========================================================================
// Narzedzia scenariuszy
// ===========================================================================
bool g_obserwacje = false;
// Wersja, ktora firmware ma zglaszac (FIRMWARE_VERSION_PANEL w src/main.cpp).
// Wpisana tu osobno celowo: zmiana wersji w firmware bez zmiany tutaj wywraca
// cztery scenariusze - trudno zapomniec o podbiciu numeru po obu stronach.
#define WERSJA_FW "v1.3.0"
int g_bledy = 0, g_sprawdzen = 0;
std::string g_katalogWyjscia;
std::string g_nazwaScenariusza;

// true od chwili, gdy firmware wywolal ESP.restart(). Na plytce program
// zaczyna sie wtedy od nowa; tutaj petla po prostu przestaje byc wolana
// (zegar plynie dalej, zeby scenariusz mogl dobiec do konca).
bool g_poRestarcie = false;

void biegnij(unsigned long ms) {
  const unsigned long koniec = millis() + ms;
  while (millis() < koniec) {
    if (g_poRestarcie) { atrapa_uplyw_czasu(5); continue; }
    try {
      LiczFirmware licz;
      loop();
    } catch (const AtrapaRestart&) {
      g_poRestarcie = true;
    }
  }
}

void zbierzNapisy(lv_obj_t* o, std::vector<std::string>& wynik) {
  if (o == nullptr || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (lv_obj_check_type(o, &lv_label_class)) {
    const char* t = lv_label_get_text(o);
    wynik.push_back(t ? t : "");
  }
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) zbierzNapisy(lv_obj_get_child(o, (int32_t)i), wynik);
}

// Teksty wszystkich WIDOCZNYCH etykiet na aktywnym ekranie LVGL.
// Pierwsza WIDOCZNA etykieta, ktorej tekst zaczyna sie od `prefiks` (albo nullptr).
lv_obj_t* znajdzEtykiete(lv_obj_t* o, const std::string& prefiks) {
  if (o == nullptr || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return nullptr;
  if (lv_obj_check_type(o, &lv_label_class)) {
    const char* t = lv_label_get_text(o);
    if (t != nullptr && std::string(t).rfind(prefiks, 0) == 0) return o;
  }
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t* w = znajdzEtykiete(lv_obj_get_child(o, (int32_t)i), prefiks);
    if (w != nullptr) return w;
  }
  return nullptr;
}

std::vector<std::string> napisyEkranu() {
  std::vector<std::string> wynik;
  zbierzNapisy(lv_scr_act(), wynik);
  zbierzNapisy(lv_layer_top(), wynik);
  return wynik;
}

int ileNapisow(const std::string& tekst) {
  int n = 0;
  for (const std::string& s : napisyEkranu())
    if (s == tekst) n++;
  return n;
}
bool ekranMa(const std::string& tekst) { return ileNapisow(tekst) > 0; }

// Liczba linii logu Seriala zawierajacych wszystkie podane fragmenty,
// liczona od pozycji `od` (w znakach) do konca.
int ileLinii(const std::string& f1_, const std::string& f2 = "", const std::string& f3 = "", size_t od = 0) {
  int n = 0;
  size_t p = od;
  const std::string& log = g_atrapa.serial;
  while (p < log.size()) {
    size_t k = log.find('\n', p);
    if (k == std::string::npos) k = log.size();
    const std::string linia = log.substr(p, k - p);
    if (linia.find(f1_) != std::string::npos && (f2.empty() || linia.find(f2) != std::string::npos) &&
        (f3.empty() || linia.find(f3) != std::string::npos))
      n++;
    p = k + 1;
  }
  return n;
}
bool logMa(const std::string& a, const std::string& b = "", const std::string& c = "", size_t od = 0) { return ileLinii(a, b, c, od) > 0; }

// Pierwsza linia logu zawierajaca fragment (do pokazania w raporcie).
std::string liniaLogu(const std::string& fragment, size_t od = 0) {
  const std::string& log = g_atrapa.serial;
  size_t p = log.find(fragment, od);
  if (p == std::string::npos) return "(brak)";
  size_t a = log.rfind('\n', p);
  a = (a == std::string::npos) ? 0 : a + 1;
  size_t b = log.find('\n', p);
  if (b == std::string::npos) b = log.size();
  return log.substr(a, b - a);
}

int ileLiniiRazem(size_t od = 0) {
  int n = 0;
  for (size_t i = od; i < g_atrapa.serial.size(); i++)
    if (g_atrapa.serial[i] == '\n') n++;
  return n;
}

void sprawdz(bool ok, const char* opis) {
  g_sprawdzen++;
  if (g_obserwacje) {
    printf("    [%s] %s\n", ok ? "tak" : "NIE", opis);
  } else {
    printf("    [%s] %s\n", ok ? " OK " : "BLAD", opis);
    if (!ok) g_bledy++;
  }
}

void info(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void info(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("    - ");
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
}

std::string bssidTekst(const uint8_t* b) {
  char t[18];
  snprintf(t, sizeof(t), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return t;
}

std::string polaczonyBssid() {
  const AtrapaAp* ap = g_atrapa.polaczonyAp();
  return ap ? bssidTekst(ap->bssid) : "(brak polaczenia)";
}

// Suma kontrolna prostokata ekranu - do sprawdzania, czy obraz w tym miejscu
// sie zmienia (animacja) czy stoi.
uint32_t sumaObszaru(int x, int y, int w, int h) {
  uint32_t c = 2166136261u;
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) { c ^= g_atrapa.ekran[yy * 480 + xx]; c *= 16777619u; }
  return c;
}

// Ile pikseli danego koloru (RGB565) jest w prostokacie ekranu.
int ilePikseliKoloru(int x, int y, int w, int h, uint16_t kolor) {
  int n = 0;
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      if (g_atrapa.ekran[yy * 480 + xx] == kolor) n++;
  return n;
}

// Ikona pompy jest niebieska (KOL_KROPLA = 0x4C9F w dashboard_lvgl_poc.cpp),
// gdy pompa pracuje, i szara, gdy stoi albo gdy panel nie ma danych.
bool ikonaPompyNiebieska() { return ilePikseliKoloru(420, 140, 54, 60, 0x4C9F) > 50; }

// Czy ikona wirnika na kafelku POMPA (prawa czesc kafelka) animuje sie:
// porownanie obrazu w kilku chwilach co 150 ms (tyle trwa jedna klatka).
bool wirnikPompySieKreci() {
  const uint32_t a = sumaObszaru(420, 140, 54, 60);
  bool zmiana = false;
  for (int i = 0; i < 4; i++) {
    biegnij(150);
    if (sumaObszaru(420, 140, 54, 60) != a) zmiana = true;
  }
  return zmiana;
}

// Zrzut tego, co firmware wyslal na ekran, jako 24-bitowy BMP.
void zrzutEkranu(const char* nazwa) {
  if (g_katalogWyjscia.empty()) return;
  const std::string sciezka = g_katalogWyjscia + "/" + g_nazwaScenariusza + "_" + nazwa + ".bmp";
  FILE* f = fopen(sciezka.c_str(), "wb");
  if (!f) return;
  const int W = 480, H = 320;
  const uint32_t rozmiarDanych = W * H * 3, rozmiarPliku = 54 + rozmiarDanych;
  uint8_t n[54] = {'B', 'M'};
  auto u32 = [&](int poz, uint32_t v) { n[poz] = v & 255; n[poz + 1] = (v >> 8) & 255; n[poz + 2] = (v >> 16) & 255; n[poz + 3] = (v >> 24) & 255; };
  u32(2, rozmiarPliku); u32(10, 54); u32(14, 40); u32(18, W); u32(22, (uint32_t)(-H));   // ujemna wysokosc = wiersze od gory
  n[26] = 1; n[28] = 24; u32(34, rozmiarDanych);
  fwrite(n, 1, 54, f);
  for (int i = 0; i < W * H; i++) {
    const uint16_t p = g_atrapa.ekran[i];
    const uint8_t bgr[3] = {(uint8_t)((p & 31) * 255 / 31), (uint8_t)(((p >> 5) & 63) * 255 / 63), (uint8_t)(((p >> 11) & 31) * 255 / 31)};
    fwrite(bgr, 1, 3, f);
  }
  fclose(f);
}

void zapiszLog() {
  if (g_katalogWyjscia.empty()) return;
  const std::string sciezka = g_katalogWyjscia + "/" + g_nazwaScenariusza + ".log";
  FILE* f = fopen(sciezka.c_str(), "w");
  if (!f) return;
  fwrite(g_atrapa.serial.data(), 1, g_atrapa.serial.size(), f);
  fclose(f);
}

const char* SSID_CENTRALI = "PiecCO_AP";
const uint8_t BSSID_PIEC[6] = {0x24, 0x6F, 0x28, 0x11, 0x22, 0x33};      // "prawdziwa centrala przy piecu"
const uint8_t BSSID_BIURKO[6] = {0x34, 0x85, 0x18, 0xAA, 0xBB, 0xCC};    // "centrala testowa na biurku"
const uint8_t BSSID_NOWA[6] = {0x7C, 0xDF, 0xA1, 0x01, 0x02, 0x03};      // "wymieniona plytka centrali"

AtrapaAp ap(const uint8_t* bssid, int kanal, int rssi, int centrala) {
  AtrapaAp a;
  a.ssid = SSID_CENTRALI;
  memcpy(a.bssid, bssid, 6);
  a.kanal = kanal;
  a.rssi = rssi;
  a.centrala = centrala;
  return a;
}

void ustaw(int idx, void (*zmiana)(Centrala&)) {
  std::lock_guard<std::mutex> blokada(g_mutex);
  zmiana(g_centrale[idx]);
}
Centrala kopia(int idx) {
  std::lock_guard<std::mutex> blokada(g_mutex);
  return g_centrale[idx];
}

// Stan plytki jak u uzytkownika: zasilanie z USB, bez ogniwa (0,28 V na
// dzielniku), dotyk skalibrowany "na zero" (panel nie widzi zadnego dotyku).
void plytkaDomyslna() {
  g_atrapa.gpio[35] = LOW;     // USB_DETECT_PIN: LOW = "USB podlaczone" (tak bylo w logu: ladowanie=1)
  g_atrapa.adcMv[34] = 142;    // BATERIA_ADC_PIN: 142 mV x 2 = 0,28 V (tak bylo w logu)
  g_atrapa.nvs["dotyk"]["rawXA"] = 100;
  g_atrapa.nvs["dotyk"]["rawYA"] = 100;
  g_atrapa.nvs["dotyk"]["rawXB"] = 100;
  g_atrapa.nvs["dotyk"]["rawYB"] = 100;
}

// ===========================================================================
// SCENARIUSZE
// ===========================================================================

// --- 1 ---------------------------------------------------------------------
// Hipoteza nr 1 dla zgloszenia "kafelki sie nie aktualizuja" (niepotwierdzona
// na sprzecie). Dwie centrale nadaja te sama siec: prawdziwa przy piecu
// (daleko, kanal 1) i testowa na biurku (obok panelu, kanal 6). Uzytkownik
// zmienia wartosci w testowej i patrzy na panel.
void scenariuszDwieCentrale() {
  g_centrale[0].tOgrz = 61.0;   // piec
  g_centrale[1].symulator = true;
  g_centrale[1].tOgrz = 20.0;   // biurko
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -78, 0));
  g_atrapa.eter.push_back(ap(BSSID_BIURKO, 6, -41, 1));

  setup();
  biegnij(4000);
  zrzutEkranu("1_po_starcie");

  info("panel polaczony z: %s (piec = %s, biurko = %s)", polaczonyBssid().c_str(), bssidTekst(BSSID_PIEC).c_str(), bssidTekst(BSSID_BIURKO).c_str());
  info("zapytania /api/status:  piec=%d  biurko=%d", kopia(0).nStatus, kopia(1).nStatus);
  sprawdz(polaczonyBssid() == bssidTekst(BSSID_BIURKO), "panel laczy sie z SILNIEJSZA centrala (ta na biurku)");
  sprawdz(g_atrapa.liczbaSkanow == 1, "przy starcie wykonal jeden skan sieci");
  sprawdz(logMa("tag=WIFI", "zdarzenie=skan_centrala", "24:6F:28:11:22:33") && logMa("tag=WIFI", "zdarzenie=skan_centrala", "34:85:18:AA:BB:CC"),
          "log: obie centrale wypisane z BSSID, kanalem i sila sygnalu");
  sprawdz(logMa("tag=WIFI", "zdarzenie=wiele_central", "liczba=2"), "log: ostrzezenie, ze sa 2 sieci o tej samej nazwie");
  sprawdz(logMa("tag=WIFI", "zdarzenie=polaczono", "bssid=34:85:18:AA:BB:CC"), "log: z ktorym BSSID sie polaczyl");
  sprawdz(logMa("tag=PANEL", "zdarzenie=centrala_rozpoznana", "typ=symulator"), "log: rozpoznal, ze odpowiada centrala testowa");
  sprawdz(ekranMa("SYMULATOR  2 CENTRALE (BB:CC)"), "ekran: znacznik \"SYMULATOR  2 CENTRALE (BB:CC)\" w naglowku");
  sprawdz(ekranMa("20°C"), "kafelek PIEC pokazuje wartosc z centrali testowej (20°C)");
  if (ekranMa("61°C")) info("kafelek PIEC pokazuje 61°C - to wartosc z centrali PRZY PIECU");

  // Uzytkownik zmienia wartosc w centrali testowej.
  ustaw(1, [](Centrala& c) { c.tOgrz = 73.0; c.tOgrzSr = 70.0; c.tBojler = 66.0; });
  biegnij(3000);
  zrzutEkranu("2_po_zmianie_w_testowej");
  sprawdz(ekranMa("73°C") && ekranMa("SR 70°C") && ekranMa("66°C"), "po zmianie w centrali testowej kafelki pokazuja nowe wartosci (73 / SR 70 / 66)");
  sprawdz(kopia(0).nStatus == 0, "prawdziwa centrala nie dostala ani jednego zapytania");
}

// --- 1b --------------------------------------------------------------------
// Te same dwie centrale, ale panel wisi przy piecu (centrala testowa daleko).
void scenariuszDwieCentralePiecBlizej() {
  g_centrale[0].tOgrz = 61.0;
  g_centrale[1].symulator = true;
  g_centrale[1].tOgrz = 20.0;
  g_atrapa.eter.push_back(ap(BSSID_BIURKO, 6, -80, 1));
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 11, -47, 0));   // kanal WYZSZY niz testowa: szybki skan trafilby na testowa
  setup();
  biegnij(4000);
  zrzutEkranu("1_po_starcie");
  info("panel polaczony z: %s", polaczonyBssid().c_str());
  sprawdz(polaczonyBssid() == bssidTekst(BSSID_PIEC), "panel laczy sie z centrala przy piecu (silniejsza), mimo ze testowa ma nizszy kanal");
  sprawdz(ekranMa("61°C"), "kafelek PIEC pokazuje wartosc z pieca (61°C)");
  sprawdz(logMa("tag=PANEL", "zdarzenie=centrala_rozpoznana", "typ=sterownik"), "log: odpowiada sterownik, nie symulator");
  sprawdz(ekranMa("2 CENTRALE (22:33)") && !ekranMa("SYMULATOR"), "ekran: znacznik \"2 CENTRALE (22:33)\", bez slowa SYMULATOR");
}

// --- 2 ---------------------------------------------------------------------
// Centrala odpowiada, ale panel nie umie odpowiedzi uzyc. Warstwa sieciowa
// widzi "sukces" (gniazdo zamkniete po odpowiedzi) - tak jak w logu z 10.11.
void scenariuszZlaOdpowiedz() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -55, 0));
  setup();
  biegnij(6000);
  sprawdz(ekranMa("55°C") && ekranMa("8.0°C"), "na starcie kafelki pokazuja dane z centrali (55°C, 8.0°C)");

  struct Przypadek { int tryb; const char* nazwa; const char* przyczyna; const char* szczegol; };
  const Przypadek przypadki[] = {
      {TRYB_NAN, "pole o wartosci nan", "przyczyna=json", "podejrzane=\".cisnienie.:nan"},
      {TRYB_HTTP500, "HTTP 500", "przyczyna=kod_http", "http=500"},
      {TRYB_PUSTA, "polaczenie zamkniete bez odpowiedzi", "przyczyna=nie_http", "bajtow=0"},
      {TRYB_UCIETA, "odpowiedz ucieta w polowie", "przyczyna=json", "json=IncompleteInput"},
  };
  static int trybDoUstawienia;
  for (const Przypadek& p : przypadki) {
    printf("  %s:\n", p.nazwa);
    const size_t od = g_atrapa.serial.size();
    const int statusowPrzed = kopia(0).nStatus;
    trybDoUstawienia = p.tryb;
    ustaw(0, [](Centrala& c) { c.trybStatus = trybDoUstawienia; c.tOgrz += 7.0; });   // centrala dalej "zyje": wartosc sie zmienia
    biegnij(13000);
    info("warstwa sieciowa w tym czasie: %d x \"sukces\" (gniazdo zamkniete po odpowiedzi), 0 bledow transportu",
         kopia(0).nStatus - statusowPrzed);
    info("%s", liniaLogu("zdarzenie=odpowiedz_nieczytelna", od).c_str());
    sprawdz(logMa("tag=STATUS lvl=ERR", p.przyczyna, p.szczegol, od), "log: wpis ERR z przyczyna i szczegolem");
    const int wpisow = ileLinii("tag=STATUS lvl=ERR", "zdarzenie=odpowiedz_nieczytelna", "", od);
    sprawdz(wpisow >= 1 && wpisow <= 2, "log nie jest zalewany (najwyzej 1 wpis na 10 s, mimo bledu co 2 s)");
    sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=dane_nieaktualne", "", od), "log: po 10 s bez danych wpis dane_nieaktualne");
    sprawdz(ileNapisow("--") >= 8 && !ekranMa("55°C"), "ekran: kafelki pokazuja \"--\", a nie ostatnie znane wartosci");
    if (p.tryb == TRYB_NAN) zrzutEkranu("1_dane_nieaktualne");

    const size_t odPowrotu = g_atrapa.serial.size();
    ustaw(0, [](Centrala& c) { c.trybStatus = TRYB_OK; });
    biegnij(4000);
    char oczekiwana[16];
    snprintf(oczekiwana, sizeof(oczekiwana), "%.0f°C", kopia(0).tOgrz);
    sprawdz(ekranMa(oczekiwana), "po naprawie: aktualna wartosc na kafelku");
    sprawdz(logMa("tag=PANEL", "zdarzenie=dane_wrocily", "", odPowrotu), "po naprawie: wpis dane_wrocily");
  }
  zrzutEkranu("2_po_powrocie");
}

// --- 3 ---------------------------------------------------------------------
// Panel wlaczony, gdy centrali nie ma (wylaczona / poza zasiegiem).
void scenariuszBrakCentraliNaStarcie() {
  setup();
  info("setup() trwal %lu ms czasu plytki", millis());
  info("watchdog zadania zadzialalby %d raz(y)%s", g_atrapa.wdtZadzialal,
       g_atrapa.wdtZadzialal ? (std::string(", pierwszy raz po ") + std::to_string(g_atrapa.wdtPierwszyRazMs) + " ms").c_str() : "");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog (15 s) NIE restartuje panelu podczas czekania na WiFi");
  sprawdz(logMa("tag=WIFI lvl=WARN", "zdarzenie=skan_brak_centrali"), "log: skan nie znalazl sieci centrali");
  sprawdz(logMa("tag=WIFI lvl=WARN", "zdarzenie=brak_polaczenia_na_starcie"), "log: brak polaczenia na starcie");
  biegnij(5000);
  sprawdz(ileNapisow("--") >= 8, "ekran: kafelki pokazuja \"--\"");
  sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=dane_nieaktualne", "wiek_s=-1"), "log: dane_nieaktualne (jeszcze nigdy nie bylo danych)");
  zrzutEkranu("1_bez_centrali");

  // Centrala wstaje.
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -60, 0));
  biegnij(45000);
  sprawdz(polaczonyBssid() == bssidTekst(BSSID_PIEC), "po wlaczeniu centrali panel laczy sie sam");
  sprawdz(logMa("tag=WIFI", "zdarzenie=reconnect_ok", "bssid=24:6F:28:11:22:33"), "log: reconnect_ok z BSSID");
  sprawdz(ekranMa("55°C"), "kafelki pokazuja dane");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal w calym scenariuszu");
}

// --- 4 ---------------------------------------------------------------------
// Centrala znika w trakcie pracy, potem wraca; na koniec wraca jako INNA
// plytka (inny BSSID - np. po wymianie albo gdy zostala tylko druga centrala).
void scenariuszZanikIPowrot() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -60, 0));
  ustaw(0, [](Centrala& c) { c.pompa = true; });
  setup();
  biegnij(6000);
  sprawdz(ekranMa("55°C"), "na starcie kafelki pokazuja dane");
  sprawdz(wirnikPompySieKreci() && ikonaPompyNiebieska(), "pompa pracuje: wirnik na kafelku POMPA sie kreci, ikona niebieska");

  printf("  centrala znika:\n");
  size_t od = g_atrapa.serial.size();
  g_atrapa.eter.clear();
  // Ostatni dobry odczyt byl najwyzej 2 s przed zanikiem, wiec po 6 s dane
  // maja najwyzej 8 s - ponizej progu 10 s.
  biegnij(6000);
  sprawdz(ekranMa("55°C"), "przez pierwsze sekundy zostaje ostatnia wartosc (prog to 10 s)");
  biegnij(7000);
  sprawdz(logMa("tag=WIFI lvl=WARN", "zdarzenie=utrata_polaczenia", "", od), "log: utrata_polaczenia");
  sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=dane_nieaktualne", "", od), "log: dane_nieaktualne");
  sprawdz(ileNapisow("--") >= 8 && !ekranMa("55°C"), "ekran: po 10 s kafelki pokazuja \"--\"");
  sprawdz(!wirnikPompySieKreci() && !ikonaPompyNiebieska(), "ekran: wirnik pompy stoi, ikona wygaszona (panel nie wie juz, czy pompa pracuje)");
  sprawdz(ileLinii("zdarzenie=dane_nieaktualne", "", "", od) == 1, "wpis dane_nieaktualne pojawia sie raz, nie co obrot petli");
  zrzutEkranu("1_po_zaniku");

  printf("  ta sama centrala wraca:\n");
  od = g_atrapa.serial.size();
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -60, 0));
  ustaw(0, [](Centrala& c) { c.tOgrz = 58.0; });
  biegnij(40000);
  sprawdz(polaczonyBssid() == bssidTekst(BSSID_PIEC) && ekranMa("58°C"), "panel laczy sie ponownie i kafelki pokazuja aktualna wartosc (58°C)");
  sprawdz(wirnikPompySieKreci() && ikonaPompyNiebieska(), "wirnik pompy znow sie kreci, ikona niebieska");
  sprawdz(logMa("tag=WIFI", "zdarzenie=reconnect_ok", "bssid=24:6F:28:11:22:33", od), "log: reconnect_ok z BSSID");
  sprawdz(logMa("tag=PANEL", "zdarzenie=dane_wrocily", "", od), "log: dane_wrocily");

  printf("  zamiast niej pojawia sie inna plytka (inny BSSID):\n");
  od = g_atrapa.serial.size();
  g_atrapa.eter.clear();
  biegnij(3000);
  g_centrale[1].tOgrz = 33.0;
  g_atrapa.eter.push_back(ap(BSSID_NOWA, 11, -50, 1));
  biegnij(120000);
  info("po 120 s panel polaczony z: %s", polaczonyBssid().c_str());
  sprawdz(polaczonyBssid() == bssidTekst(BSSID_NOWA) && ekranMa("33°C"), "panel laczy sie z nowa centrala i pokazuje jej dane (33°C)");
  sprawdz(logMa("tag=WIFI lvl=WARN", "zdarzenie=porzucam_bssid", "", od), "log: porzucam_bssid");
  info("WiFi.begin(): %d razy, w tym %d z BSSID (z tego %d z pelnym skanem kanalow)", g_atrapa.liczbaBegin, g_atrapa.liczbaBeginZBssid,
       g_atrapa.liczbaBeginZBssidPelnySkan);
  sprawdz(g_atrapa.liczbaBeginZBssid > 0 && g_atrapa.liczbaBeginZBssidPelnySkan == 0,
          "laczenie ze ZNANA centrala (po BSSID) nie robi pelnego skanu kanalow - szybkie jak w v1.1.0");
}

// --- 5 ---------------------------------------------------------------------
// Jedno nieudane polecenie z dotyku (POST), np. dotkniecie w chwili, gdy
// centrala nie odpowiada.
void scenariuszNieudanyPost() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -60, 0));
  setup();
  biegnij(4000);
  ustaw(0, [](Centrala& c) { c.trybPost = TRYB_MILCZY; });
  const size_t od = g_atrapa.serial.size();
  pompaManualWyslij("wl");
  biegnij(4000);
  const int linii = ileLinii("[KOMENDA] POST nieudany", "", "", od);
  info("linii \"[KOMENDA] POST nieudany\" w ciagu 4 s po jednym nieudanym POST: %d", linii);
  sprawdz(kopia(0).nPost == 1, "centrala dostala dokladnie jedno polecenie");
  sprawdz(linii == 1, "jeden nieudany POST = jedna linia w logu");
  sprawdz(ekranMa("55°C"), "odczyt statusu dziala dalej");
}

// --- 6 ---------------------------------------------------------------------
// Zwykla praca przez ponad minute: ile panel pisze do logu i co pokazuje
// wskaznik baterii przy zasilaniu z samego USB.
void scenariuszBateriaILog() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -60, 0));
  setup();
  const size_t od = g_atrapa.serial.size();
  const unsigned long start = millis();
  // Linia tag=PANEL wychodzi w 5., 35. i 65. sekundzie od wlaczenia.
  biegnij(66000 - millis());
  const int linii = ileLiniiRazem(od);
  info("linii logu w ciagu %lu s zwyklej pracy: %d (w tym [NET-DIAG]: %d)", (millis() - start) / 1000, linii, ileLinii("[NET-DIAG]", "", "", od));
  info("%s", liniaLogu("tag=PANEL fw=", od).c_str());
  info("%s", liniaLogu("tag=BATERIA", od).c_str());
  sprawdz(ileLinii("[NET-DIAG]", "", "", od) == 0, "brak linii [NET-DIAG] przy poprawnej pracy");
  sprawdz(ileLinii("tag=PANEL fw=", "", "", od) == 3, "linia tag=PANEL: 5 s po wlaczeniu i potem co 30 s (3 w ciagu 66 s)");
  sprawdz(logMa("tag=PANEL fw=" WERSJA_FW, "bssid=24:6F:28:11:22:33", "centrala=sterownik", od), "linia tag=PANEL podaje wersje, BSSID i rodzaj centrali");
  sprawdz(logMa("tag=PANEL fw=", "bez_danych_s=", "przyczyna=- wymuszen=0 rozl_kod=0 rozl_n=0 autorestartow=0", od),
          "linia tag=PANEL: przy poprawnej pracy nic nie brakuje, zero rozlaczen i restartow");
  sprawdz(kopia(0).ostatniStatusSciezka.find("panel_fw=" WERSJA_FW "&reset_powod=poweron") != std::string::npos,
          "centrala dostaje w zapytaniu wersje panelu i zwykly powod startu (poweron)");
  sprawdz(logMa("tag=PANEL fw=", "status_ok=15", "status_blad=0", od), "linia tag=PANEL liczy poprawne odczyty (15 na 30 s)");
  sprawdz(logMa("tag=PANEL fw=", "t_ogrz=55.0", "klapa=40", od), "linia tag=PANEL pokazuje wartosci, ktore panel ma w pamieci");
  sprawdz(linii <= 12, "najwyzej 12 linii logu na minute zwyklej pracy");
  sprawdz(ileLinii("tag=BATERIA", "zdarzenie=brak_ogniwa", "", od) == 1, "log: brak_ogniwa zgloszony raz, nie co 30 s");
  sprawdz(ekranMa("USB") && !ekranMa("0%"), "wskaznik baterii: \"USB\" zamiast \"0%\"");
  sprawdz(!ekranMa("SYMULATOR") && !logMa("zdarzenie=wiele_central"), "jedna zwykla centrala: brak znacznika w naglowku");
  zrzutEkranu("1_usb");

  // Podlaczone ogniwo 3,90 V (1950 mV na dzielniku).
  g_atrapa.adcMv[34] = 1950;
  biegnij(31000);
  info("%s", liniaLogu("tag=BATERIA napiecie=3.90V").c_str());
  sprawdz(logMa("tag=BATERIA napiecie=3.90V", "procent="), "z ogniwem: zwykla linia z napieciem i procentami");
  sprawdz(!ekranMa("USB"), "z ogniwem: wskaznik pokazuje procenty");
}

// --- 7 ---------------------------------------------------------------------
// Skan WiFi, ktory sie nie konczy (blad sterownika radia).
void scenariuszSkanZawieszony() {
  g_centrale[1].symulator = true;
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -78, 0));
  g_atrapa.eter.push_back(ap(BSSID_BIURKO, 6, -41, 1));
  g_atrapa.skanZawieszony = true;
  setup();
  biegnij(4000);
  info("setup() + 4 s: %lu ms; esp_wifi_scan_stop() wolane %d raz(y)", millis(), g_atrapa.liczbaStopSkanu);
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal mimo zawieszonego skanu");
  sprawdz(g_atrapa.liczbaStopSkanu == 1, "po 6 s skan zostal przerwany");
  sprawdz(logMa("tag=WIFI lvl=WARN", "zdarzenie=skan_brak_centrali", "wynik_skanu=-2"), "log: skan nieudany (wynik_skanu=-2)");
  sprawdz(polaczonyBssid() == bssidTekst(BSSID_BIURKO), "mimo to panel laczy sie z silniejsza centrala (wybor robi sterownik WiFi)");
  sprawdz(ekranMa("SYMULATOR"), "ekran: znacznik SYMULATOR");
}

// --- 8 ---------------------------------------------------------------------
// Sytuacja z logu prawdziwego panelu z 2026-11-06 (README centrali testowej,
// pkt 5d): /api/status dziala, a /api/ustawienia-get konczy sie "timeout
// calkowity". Tamten panel po serii takich bledow sie wywrocil (Guru
// Meditation, StoreProhibited). Tu: trzy minuty takiej pracy.
void scenariuszUstawieniaMilcza() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -60, 0));
  ustaw(0, [](Centrala& c) { c.trybUstawienia = TRYB_MILCZY; });
  setup();
  const size_t od = g_atrapa.serial.size();
  biegnij(180000);
  const int zapytan = kopia(0).nUstawienia;
  const int linii = ileLinii("[USTAWIENIA] GET nieudany", "", "", od);
  info("przez 3 min: %d zapytan /api/ustawienia-get bez odpowiedzi, %d linii \"[USTAWIENIA] GET nieudany\"", zapytan, linii);
  info("%s", liniaLogu("[USTAWIENIA] GET nieudany", od).c_str());
  sprawdz(zapytan >= 30, "panel ponawia /api/ustawienia-get co 5 s (co najmniej 30 prob w 3 min)");
  sprawdz(linii >= 1 && linii <= zapytan + 1, "najwyzej jedna linia bledu na jedno nieudane zapytanie");
  sprawdz(logMa("tag=PANEL fw=", "ust_ok=0", "ust_blad=", od) && !logMa("tag=PANEL fw=", "ust_blad=0", "", od),
          "linia tag=PANEL liczy nieudane odczyty ustawien (ust_ok=0, ust_blad>0)");
  ustaw(0, [](Centrala& c) { c.tOgrz = 64.0; });
  biegnij(3000);
  sprawdz(ekranMa("64°C"), "kafelki statusu dzialaja dalej i pokazuja aktualna wartosc (64°C)");
  sprawdz(g_atrapa.wdtZadzialal == 0 && g_atrapa.restarty == 0, "bez restartu i bez zadzialania watchdoga");

  const size_t odPowrotu = g_atrapa.serial.size();
  ustaw(0, [](Centrala& c) { c.trybUstawienia = TRYB_OK; });
  // Linia tag=PANEL obejmuje 30 s; pierwsza po zmianie zawiera jeszcze stare
  // bledy, dopiero kolejna cale "zdrowe" okno (6 odczytow co 5 s).
  biegnij(66000);
  info("%s", liniaLogu("ust_ok=6", odPowrotu).substr(0, 200).c_str());
  sprawdz(logMa("tag=PANEL fw=", "ust_ok=6", "ust_blad=0", odPowrotu),
          "gdy centrala znow odpowiada: ustawienia sa odczytywane (ust_ok=6, ust_blad=0 na 30 s)");
}

// --- 9 ---------------------------------------------------------------------
// Stanowisko z logow uzytkownika z 2026-11-11: centrala to ten sam firmware
// (3.58.3) na golej plytce, bez czujnikow. Linia z jej logu:
//   tag=STATUS piec=-1.0C piecSr=0.0C bojler=-1.0C panel=-1.0C pom=-1.0C
//   zewn=18.5C cisnienie=0.0hPa wilgotnosc=0.0% klapa=0st syberek=16st
//   pompa=OFF dym=2413
// czyli wszedzie "brak czujnika" (-1), a jedna wartosc (zewn) pochodzi z
// symulacji wlaczonej w panelu WWW centrali.
void scenariuszGolaPlytka() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 1, -45, 0));
  ustaw(0, [](Centrala& c) {
    c.tOgrz = -1.0; c.tOgrzSr = 0.0; c.tBojler = -1.0; c.tPanel = -1.0; c.tPokoj = -1.0; c.tTrociny = -1.0; c.tPowrot = -1.0;
    c.tZewn = 18.5; c.symulacja = SYM_ZEWN;
    c.cisnienie = 0.0; c.wilgotnosc = 0.0; c.klapa = 0; c.syberka = 16; c.pompa = false;
    c.dym = 2413; c.dymWlaczony = false;
    c.godzina = 20; c.minuta = 41; c.dzien = 11; c.miesiac = 11; c.rok = 2026;
  });
  setup();
  biegnij(5000);
  zrzutEkranu("1_po_starcie");
  std::string napisy;
  for (const std::string& t : napisyEkranu()) { if (!napisy.empty()) napisy += " | "; napisy += t; }
  info("napisy na ekranie: %s", napisy.c_str());
  info("%s", liniaLogu("tag=PANEL fw=").c_str());
  sprawdz(ekranMa("18.5°C"), "kafelek ZEWN pokazuje wartosc z symulacji centrali (18.5°C)");
  sprawdz(ileNapisow("-1°C") == 3 && ekranMa("-1.0°C") && ekranMa("SR 0°C"), "kafelki bez czujnika pokazuja to, co podaje centrala: -1 (BOJLER, PANEL, PIEC), -1.0 (POKOJ), SR 0");
  sprawdz(ekranMa("2413") && ekranMa("20:41") && ekranMa("11.11.2026"), "DYM 2413, godzina 20:41, data 11.11.2026 - jak w centrali");
  sprawdz(logMa("tag=PANEL", "zdarzenie=symulacja_centrali", "czujniki=zewn"), "log: centrala zglasza symulacje czujnika zewn");
  sprawdz(ekranMa("SYMULACJA: zewn"), "naglowek: \"SYMULACJA: zewn\"");
  sprawdz(logMa("tag=PANEL fw=", "t_zewn=18.5", "sym=zewn") && logMa("tag=PANEL fw=", "t_ogrz=-1.0", "dym=2413") &&
              logMa("tag=PANEL fw=", "t_pokoj=-1.0", "syberka=16"),
          "linia tag=PANEL podaje te same wartosci co linia tag=STATUS centrali");

  printf("  zmiana symulowanej temperatury w centrali (18.5 -> 25.0):\n");
  ustaw(0, [](Centrala& c) { c.tZewn = 25.0; });
  biegnij(3000);
  sprawdz(ekranMa("25.0°C") && !ekranMa("18.5°C"), "kafelek ZEWN pokazuje 25.0°C najdalej po 3 s");

  printf("  wlaczenie symulacji drugiego czujnika (pokoj = 22.5):\n");
  size_t od = g_atrapa.serial.size();
  ustaw(0, [](Centrala& c) { c.tPokoj = 22.5; c.symulacja |= SYM_POKOJ; });
  biegnij(3000);
  zrzutEkranu("2_dwie_symulacje");
  sprawdz(ekranMa("22.5°C"), "kafelek POKOJ pokazuje 22.5°C");
  sprawdz(ekranMa("SYMULACJA: zewn, pokoj"), "naglowek: \"SYMULACJA: zewn, pokoj\"");
  sprawdz(logMa("tag=PANEL", "zdarzenie=symulacja_centrali", "czujniki=zewn,pokoj", od), "log: symulacja_centrali czujniki=zewn,pokoj");

  printf("  wylaczenie symulacji:\n");
  od = g_atrapa.serial.size();
  ustaw(0, [](Centrala& c) { c.tZewn = -1.0; c.tPokoj = -1.0; c.symulacja = 0; });
  biegnij(3000);
  sprawdz(!ekranMa("SYMULACJA: zewn, pokoj") && znajdzEtykiete(lv_scr_act(), "SYMULACJA") == nullptr, "znacznik znika z naglowka");
  sprawdz(logMa("tag=PANEL", "zdarzenie=symulacja_centrali", "czujniki=brak", od), "log: symulacja_centrali czujniki=brak");
  sprawdz(ileNapisow("-1.0°C") == 2, "ZEWN i POKOJ wracaja do -1.0 (brak czujnika)");

  // Najdluzszy mozliwy znacznik musi zmiescic sie miedzy wskaznikiem baterii
  // (konczy sie ok. x=75) a ikonami po prawej (zaczynaja sie ok. x=400).
  if (lvgl_dashboard_poc_aktualizuj_centrale != nullptr) {
    lvgl_dashboard_poc_aktualizuj_centrale("SYMULACJA: zewn +9  2 CENTRALE (BB:CC)");
    biegnij(200);
    lv_obj_t* e = znajdzEtykiete(lv_scr_act(), "SYMULACJA: zewn +9");
    lv_area_t a = {0, 0, 0, 0};
    if (e != nullptr) lv_obj_get_coords(e, &a);
    info("najdluzszy znacznik zajmuje x=%d..%d", (int)a.x1, (int)a.x2);
    zrzutEkranu("3_najdluzszy_znacznik");
    sprawdz(e != nullptr && a.x1 >= 80 && a.x2 <= 395, "najdluzszy znacznik miesci sie w naglowku (x od 80 do 395)");
    // v1.2.1: najdluzszy znacznik braku danych (same wielkie litery - szersze).
    lvgl_dashboard_poc_aktualizuj_centrale("RESTART ZA 10 s: CENTRALA MILCZY");
    biegnij(200);
    e = znajdzEtykiete(lv_scr_act(), "RESTART ZA 10 s");
    a = {0, 0, 0, 0};
    if (e != nullptr) lv_obj_get_coords(e, &a);
    info("najdluzszy znacznik braku danych zajmuje x=%d..%d", (int)a.x1, (int)a.x2);
    zrzutEkranu("4_najdluzszy_znacznik_braku_danych");
    sprawdz(e != nullptr && a.x1 >= 80 && a.x2 <= 395, "znacznik \"RESTART ZA 10 s: CENTRALA MILCZY\" miesci sie w naglowku");
    lvgl_dashboard_poc_aktualizuj_centrale("");
  } else {
    sprawdz(false, "najdluzszy znacznik miesci sie w naglowku (x od 80 do 395)");
  }
}


// ===========================================================================
// v1.2.1: panel bez danych ratuje sie sam
// ===========================================================================
// Wartosci z logu centrali z 2026-11-12 (gola plytka, symulacja pieca 134 C
// i temperatury zewnetrznej 49 C, alarm przegrzania, pompa wlaczona).
void centralaJakWLogu1112(Centrala& c) {
  c.tOgrz = 134.0; c.tOgrzSr = 0.0; c.tBojler = -1.0; c.tPanel = -1.0; c.tPokoj = -1.0; c.tZewn = 49.0;
  c.cisnienie = 0.0; c.wilgotnosc = 0.0; c.klapa = 0; c.syberka = 0; c.dym = 9;
  c.pompa = true; c.alarmOgrzewanie = true; c.symulacja = SYM_OGRZ | SYM_ZEWN;
  c.godzina = 8; c.minuta = 32; c.dzien = 12; c.miesiac = 11; c.rok = 2026;
}

// Tekst znacznika na srodku naglowka (pierwsza widoczna etykieta zaczynajaca
// sie od jednego z podanych poczatkow) albo "".
std::string znacznikNaglowka() {
  static const char* const POCZATKI[] = {"BRAK WIFI", "CENTRALA MILCZY", "ZLA ODPOWIEDZ", "BRAK PAMIECI", "RESTART ZA", "SYMULA", "2 CENTRALE"};
  for (const char* p : POCZATKI) {
    lv_obj_t* e = znajdzEtykiete(lv_scr_act(), p);
    if (e != nullptr) return lv_label_get_text(e);
  }
  return "";
}

// Klikniecie przycisku z podanym napisem na aktywnym ekranie (jak dotyk).
bool kliknij(const char* napis) {
  lv_obj_t* e = znajdzEtykiete(lv_scr_act(), napis);
  if (e == nullptr) return false;
  LiczFirmware licz;
  lv_event_send(lv_obj_get_parent(e), LV_EVENT_CLICKED, nullptr);
  return true;
}

#ifdef DIAG_MA_RATUNEK
// Stan pamieci RTC "po samoczynnym restarcie" - tak, jak zostawia go
// panelAutorestart() w main.cpp.
void pamiecPoAutorestarcie(uint8_t kolejnych, diag::Przyczyna przyczyna, uint8_t kodRozlaczenia, uint8_t alarmy = 0) {
  rtcAutorestart.wyczysc();
  rtcAutorestart.alarmy = alarmy;
  rtcAutorestart.oczekuje = 1;
  rtcAutorestart.kolejnych = kolejnych;
  rtcAutorestart.seria = kolejnych;                                         // domyslnie: poczatek serii,
  rtcAutorestart.zgloszonyNr = (kolejnych > 0) ? (uint8_t)(kolejnych - 1) : 0;   // poprzednie restarty zgloszone
  rtcAutorestart.lacznie = kolejnych;
  rtcAutorestart.przyczyna = (uint8_t)przyczyna;
  rtcAutorestart.kodRozlaczenia = kodRozlaczenia;
  rtcAutorestart.bezDanychS = 45;
  rtcAutorestart.heapKb = 61;
  rtcAutorestart.blokKb = 31;
  rtcAutorestart.bladEtap = 'n';
  rtcAutorestart.bladNumer = 118;   // na ESP32: EHOSTUNREACH
  rtcAutorestart.zapieczetuj();
  g_atrapa.powodResetu = 3;   // ESP_RST_SW
}
#endif

// --- 11 --------------------------------------------------------------------
// Centrala restartuje sie tak szybko, ze radio panelu nie zauwaza przerwy:
// panel dalej uwaza sie za polaczony, ale centrala go juz nie zna i nie
// odpowiada. To PRZYPUSZCZALNY mechanizm zgloszenia z 2026-11-12 (log centrali
// zaczyna sie ok. 5 minut po jej starcie) - na plytce niepotwierdzony.
// v1.2.0 w tym stanie: "--" na kafelkach az do recznego resetu panelu.
void scenariuszMartwePolaczenie() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  ustaw(0, centralaJakWLogu1112);
  setup();
  biegnij(5000);
  sprawdz(ekranMa("PRZEGRZANIE") && ekranMa("Piec CO powyzej progu"), "alarm przegrzania z centrali: panel pokazuje ekran alarmu");
  zrzutEkranu("1_alarm");
  sprawdz(kliknij("ZAMKNIJ"), "ekran alarmu ma przycisk ZAMKNIJ");
  biegnij(3000);
  sprawdz(ekranMa("134°C") && ekranMa("49.0°C") && ekranMa("SYMULACJA: zewn, piec"),
          "po ZAMKNIJ: dashboard z wartosciami centrali (PIEC 134, ZEWN 49.0) i znacznikiem symulacji");
  ustaw(0, [](Centrala& c) { c.tZewn = 21.5; c.dym = 415; });
  biegnij(3000);
  sprawdz(ekranMa("21.5°C") && ekranMa("415"), "przy aktywnym alarmie kafelki dalej ida za centrala (ZEWN 21.5, DYM 415)");
  zrzutEkranu("2_dashboard_przy_alarmie");

  printf("  centrala restartuje sie (panel nie zauwaza przerwy w nadawaniu):\n");
  size_t od = g_atrapa.serial.size();
  const unsigned long chwilaRestartu = millis();
  g_atrapa.eter[0].wcielenie++;
  // Po restarcie centrala nie ma juz symulacji; uzytkownik ustawia nowa wartosc.
  ustaw(0, [](Centrala& c) { c.tOgrz = 77.0; c.tZewn = -1.0; c.alarmOgrzewanie = false; c.pompa = false; c.symulacja = SYM_OGRZ; });
  biegnij(10500);
  info("naglowek 10,5 s po restarcie centrali: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(ileNapisow("--") >= 8 && !ekranMa("134°C"), "po 10 s bez odpowiedzi kafelki pokazuja \"--\"");
  sprawdz(znacznikNaglowka().rfind("CENTRALA MILCZY", 0) == 0, "naglowek mowi, czego brakuje: \"CENTRALA MILCZY <sekundy> s\"");
  sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=dane_nieaktualne", "przyczyna=milczy", od), "log: dane_nieaktualne przyczyna=milczy");
  zrzutEkranu("3_centrala_milczy");

  // Dane wracaja: liczymy, po ilu sekundach od restartu centrali.
  unsigned long wrocilyPo = 0;
  for (int i = 0; i < 300 && wrocilyPo == 0; i++) {
    biegnij(100);
    if (ekranMa("77°C")) wrocilyPo = millis() - chwilaRestartu;
  }
  info("kafelki znow pokazuja dane centrali po %lu ms od jej restartu", wrocilyPo);
  info("%s", liniaLogu("zdarzenie=wymuszam_polaczenie", od).c_str());
  sprawdz(logMa("tag=WIFI lvl=WARN", "zdarzenie=wymuszam_polaczenie", "nr=1 szukam=ten_sam_bssid", od),
          "log: panel sam rozlacza sie i laczy od nowa (wymuszam_polaczenie nr=1)");
  sprawdz(logMa("tag=WIFI", "zdarzenie=rozlaczono", "kod=8 nazwa=ASSOC_LEAVE", od), "log: kod rozlaczenia WiFi (8 = na wlasne zyczenie)");
  sprawdz(logMa("tag=WIFI", "zdarzenie=reconnect_ok", "bssid=24:6F:28:11:22:33", od), "log: reconnect_ok");
  sprawdz(wrocilyPo > 0 && wrocilyPo <= 20000, "kafelki pokazuja nowa wartosc centrali (77°C) najdalej 20 s po jej restarcie");
  biegnij(1000);
  sprawdz(znacznikNaglowka() == "SYMULACJA: piec", "naglowek wraca do zwyklego znacznika (\"SYMULACJA: piec\")");
  sprawdz(logMa("tag=PANEL", "zdarzenie=dane_wrocily", "", od), "log: dane_wrocily");
  sprawdz(g_atrapa.restarty == 0, "restart panelu nie byl potrzebny");
  zrzutEkranu("4_po_ponownym_polaczeniu");
}

// --- 12 --------------------------------------------------------------------
// WiFi dziala, ale centrala trwale nie odpowiada na /api/status i ponowne
// polaczenie nic nie daje. Panel ma zrobic to, co dotad robil uzytkownik:
// zrestartowac sie - po 45 s, nie wczesniej.
void scenariuszAutorestartCentralaMilczy() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  setup();
  biegnij(6000);
  sprawdz(ekranMa("55°C"), "na starcie kafelki pokazuja dane");

  size_t od = g_atrapa.serial.size();
  const unsigned long start = millis();
  ustaw(0, [](Centrala& c) { c.trybStatus = TRYB_MILCZY; });
  biegnij(33000);
  info("naglowek po 33 s milczenia: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(ileLinii("zdarzenie=wymuszam_polaczenie", "", "", od) == 2 &&
              logMa("zdarzenie=wymuszam_polaczenie", "nr=1 szukam=ten_sam_bssid", "", od) &&
              logMa("zdarzenie=wymuszam_polaczenie", "nr=2 szukam=od_nowa", "", od),
          "dwa wymuszone polaczenia: z tym samym nadajnikiem, potem szukanie sieci od nowa - i nie wiecej");
  sprawdz(g_atrapa.restarty == 0, "przez pierwsze 33 s panel sie nie restartuje");
  sprawdz(znacznikNaglowka().rfind("CENTRALA MILCZY", 0) == 0, "naglowek: \"CENTRALA MILCZY <sekundy> s\"");
  biegnij(5000);
  info("naglowek po 38 s milczenia: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(znacznikNaglowka().rfind("RESTART ZA ", 0) == 0 && znacznikNaglowka().find(": CENTRALA MILCZY") != std::string::npos,
          "ostatnie 10 s: naglowek odlicza \"RESTART ZA <sekundy> s: CENTRALA MILCZY\"");
  zrzutEkranu("1_restart_za");
  sprawdz(g_atrapa.restarty == 0, "po 38 s jeszcze bez restartu");
  unsigned long restartPo = 0;
  for (int i = 0; i < 200 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  info("ESP.restart() po %lu ms od chwili, gdy centrala zamilkla", restartPo);
  info("%s", liniaLogu("zdarzenie=autorestart", od).c_str());
  sprawdz(restartPo >= 42000 && restartPo <= 46500, "restart po ok. 45 s bez danych (liczone od ostatniego poprawnego statusu)");
  sprawdz(g_atrapa.restarty == 1, "ESP.restart() wywolany dokladnie raz");
  sprawdz(logMa("tag=PANEL lvl=ERR", "zdarzenie=autorestart", "przyczyna=milczy", od) && logMa("zdarzenie=autorestart", "nr_z_rzedu=1", "wifi=1", od),
          "log: autorestart przyczyna=milczy nr_z_rzedu=1 wifi=1");
#ifdef DIAG_MA_RATUNEK
  sprawdz(MA_RATUNEK && rtcAutorestart.wazna() && rtcAutorestart.oczekuje == 1 && rtcAutorestart.kolejnych == 1 &&
              rtcAutorestart.przyczyna == diag::PRZYCZYNA_MILCZY && rtcAutorestart.bezDanychS >= 44 && rtcAutorestart.bezDanychS <= 46 &&
              rtcAutorestart.bladEtap == 'c' && rtcAutorestart.bladNumer == 0,
          "pamiec RTC: zapisane, ze to samoczynny restart nr 1, przyczyna, czas bez danych i ostatni blad (limit czasu odpowiedzi)");
#else
  sprawdz(false, "pamiec RTC: zapisane, ze to samoczynny restart nr 1, przyczyna i czas bez danych");
#endif
}

// --- 13 --------------------------------------------------------------------
// Panel wlaczony bez centrali w zasiegu (np. centrala wylaczona).
void scenariuszAutorestartBrakWifi() {
  setup();
  const unsigned long start = millis();
  info("setup() trwal %lu ms (czekanie na WiFi)", start);
  biegnij(2000);
  info("naglowek 2 s po starcie petli: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(znacznikNaglowka().rfind("BRAK WIFI", 0) == 0, "naglowek: \"BRAK WIFI <sekundy> s\"");
  sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=dane_nieaktualne", "przyczyna=wifi"), "log: dane_nieaktualne przyczyna=wifi");
  g_atrapa.zglosRozlaczenie(201);   // radio: sieci centrali nie ma w eterze
  biegnij(39000);
  sprawdz(logMa("tag=WIFI", "zdarzenie=rozlaczono", "kod=201 nazwa=NO_AP_FOUND"), "log: kod rozlaczenia zgloszony przez radio (201 = NO_AP_FOUND)");
  sprawdz(!logMa("zdarzenie=wymuszam_polaczenie"), "bez WiFi nie ma czego \"wymuszac\" - dziala zwykle ponawianie polaczenia");
  sprawdz(g_atrapa.restarty == 0, "przez pierwsze 41 s pracy petli panel sie nie restartuje");
  sprawdz(znacznikNaglowka().rfind("RESTART ZA ", 0) == 0 && znacznikNaglowka().find(": BRAK WIFI") != std::string::npos,
          "ostatnie 10 s: \"RESTART ZA <sekundy> s: BRAK WIFI\"");
  unsigned long restartPo = 0;
  for (int i = 0; i < 100 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  info("ESP.restart() po %lu ms pracy petli", restartPo);
  info("%s", liniaLogu("zdarzenie=autorestart").c_str());
  sprawdz(restartPo >= 44000 && restartPo <= 46500, "restart po 45 s pracy petli bez danych");
  sprawdz(logMa("zdarzenie=autorestart", "przyczyna=wifi", "rozl_kod=201 rozl_nazwa=NO_AP_FOUND"), "log: autorestart przyczyna=wifi z kodem rozlaczenia");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog zadania nie zadzialal");
}

// --- 14 --------------------------------------------------------------------
// Start PO samoczynnym restarcie: panel mowi centrali, dlaczego sie
// zrestartowal, a po pierwszym poprawnym statusie liczy restarty od nowa.
void scenariuszStartPoAutorestarcie() {
#ifdef DIAG_MA_RATUNEK
  if (!MA_RATUNEK) { sprawdz(false, "firmware ma pamiec autorestartu"); return; }
  pamiecPoAutorestarcie(1, diag::PRZYCZYNA_WIFI, 201);
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  setup();
  biegnij(3000);
  info("%s", liniaLogu("zdarzenie=start_po_autorestarcie").c_str());
  sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=start_po_autorestarcie", "zglaszam_centrali=auto_wifi_r201_n1_t45_h61_b31_en118"),
          "log: start_po_autorestarcie z tokenem dla centrali");
  {
    const Centrala c = kopia(0);
    for (const std::string& powod : c.resetDiag) info("centrala pisze: tag=RESET-DIAG ... nietypowy_restart_panelu powod=%s", powod.c_str());
    sprawdz(c.resetDiag.size() == 1 && c.resetDiag[0] == "auto_wifi_r201_n1_t45_h61_b31_en118",
            "centrala dostaje reset_powod=auto_wifi_r201_n1_t45_h61_b31_en118 i pisze o nim jedna linie tag=RESET-DIAG");
    info("pozniejsze zapytania: %s", c.ostatniStatusSciezka.c_str());
    sprawdz(c.ostatniStatusSciezka == "/api/status?panel_fw=" WERSJA_FW "&reset_powod=sw" && logMa("tag=PANEL", "zdarzenie=autorestart_zgloszony"),
            "po pierwszej odpowiedzi centrali panel zglasza dalej zwykle reset_powod=sw");
  }
  sprawdz(ekranMa("55°C"), "kafelki pokazuja dane");
  sprawdz(rtcAutorestart.wazna() && rtcAutorestart.oczekuje == 0 && rtcAutorestart.kolejnych == 1 && rtcAutorestart.lacznie == 1 &&
              rtcAutorestart.zgloszonyNr == 1,
          "po pierwszych poprawnych statusach licznik restartow z rzedu jeszcze sie nie zeruje; zgloszenie odnotowane w pamieci RTC");
  biegnij(30000);
  sprawdz(logMa("tag=PANEL fw=", "autorestartow=1"), "linia tag=PANEL: autorestartow=1 (od wlaczenia zasilania)");
  // Centrala restartuje sie (zapomina, co panel zglaszal) - nie moze oglosic
  // tego samego restartu panelu drugi raz.
  ustaw(0, [](Centrala& c) { c.ostatniPowodRestartu.clear(); });
  biegnij(95000);
  sprawdz(kopia(0).resetDiag.size() == 1, "po restarcie centrali ten sam restart panelu nie jest oglaszany drugi raz");
  sprawdz(rtcAutorestart.wazna() && rtcAutorestart.kolejnych == 0 && rtcAutorestart.lacznie == 1 && rtcAutorestart.seria == 1 &&
              logMa("tag=PANEL", "zdarzenie=autorestart_licznik_wyzerowany", "z_rzedu=1->0 w_serii=1->1"),
          "po 2 minutach nieprzerwanie poprawnych danych licznik restartow z rzedu wraca do zera (seria trwa dalej)");
  sprawdz(g_atrapa.restarty == 0, "przy dzialajacej centrali kolejnego restartu nie ma");
#else
  sprawdz(false, "firmware ma pamiec autorestartu");
#endif
}

// --- 14b -------------------------------------------------------------------
// Lacznosc rwie sie raz za razem: to juz 5. samoczynny restart w serii, a o 3.
// centrala wie. Tego restartu panel nie zglasza (zglaszalby dopiero 6.) -
// inaczej centrala wysylalaby wiadomosc po kazdym.
void scenariuszSeriaRestartow() {
#ifdef DIAG_MA_RATUNEK
  if (!MA_RATUNEK) { sprawdz(false, "firmware ma pamiec autorestartu"); return; }
  pamiecPoAutorestarcie(1, diag::PRZYCZYNA_MILCZY, 8);
  rtcAutorestart.seria = 5;
  rtcAutorestart.zgloszonyNr = 3;
  rtcAutorestart.lacznie = 5;
  rtcAutorestart.zapieczetuj();
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  setup();
  biegnij(6000);
  info("%s", liniaLogu("zdarzenie=start_po_autorestarcie").c_str());
  sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=start_po_autorestarcie", "w_serii=5 zgloszony_nr=3") && logMa("zglaszam_centrali=sw"),
          "log panelu: start po 5. restarcie w serii, centrali idzie zwykle sw");
  {
    const Centrala c = kopia(0);
    info("zapytanie, ktore dostala centrala: %s", c.ostatniStatusSciezka.c_str());
    sprawdz(c.nStatus >= 2 && c.resetDiag.empty() && c.ostatniStatusSciezka == "/api/status?panel_fw=" WERSJA_FW "&reset_powod=sw",
            "centrala nie pisze o tym restarcie i nie wysyla wiadomosci");
  }
  sprawdz(ekranMa("55°C"), "kafelki pokazuja dane");
  sprawdz(rtcAutorestart.wazna() && rtcAutorestart.seria == 5 && rtcAutorestart.zgloszonyNr == 3, "pamiec RTC: seria 5, zgloszony nadal 3.");
  sprawdz(diag::zglosicRestart(6, 3) && !diag::zglosicRestart(5, 3) && !diag::zglosicRestart(4, 3), "nastepny zglaszany bedzie 6. restart serii");
#else
  sprawdz(false, "firmware ma pamiec autorestartu");
#endif
}

// --- 15 --------------------------------------------------------------------
// Drugi samoczynny restart z rzedu (centrala nadal nieobecna): prog rosnie
// z 45 s do 90 s, zeby panel nie restartowal sie w kolko.
void scenariuszAutorestartKolejny() {
#ifdef DIAG_MA_RATUNEK
  if (!MA_RATUNEK) { sprawdz(false, "firmware ma pamiec autorestartu"); return; }
  pamiecPoAutorestarcie(1, diag::PRZYCZYNA_WIFI, 201);
  setup();
  const unsigned long start = millis();
  biegnij(60000);
  info("naglowek po 60 s: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(g_atrapa.restarty == 0, "po pierwszym samoczynnym restarcie panel nie restartuje sie ponownie po 45 s (ani po 60 s)");
  sprawdz(znacznikNaglowka().rfind("BRAK WIFI", 0) == 0, "naglowek: \"BRAK WIFI <sekundy> s\"");
  unsigned long restartPo = 0;
  for (int i = 0; i < 400 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  info("drugi restart po %lu ms pracy petli", restartPo);
  sprawdz(restartPo >= 89000 && restartPo <= 91500, "drugi restart dopiero po 90 s");
  sprawdz(rtcAutorestart.wazna() && rtcAutorestart.oczekuje == 1 && rtcAutorestart.kolejnych == 2 && rtcAutorestart.lacznie == 2,
          "pamiec RTC: to juz drugi restart z rzedu (nastepny prog: 3 min)");
  sprawdz(logMa("zdarzenie=autorestart", "nr_z_rzedu=2", "przyczyna=wifi"), "log: autorestart nr_z_rzedu=2");
#else
  sprawdz(false, "firmware ma pamiec autorestartu");
#endif
}

// --- 16 --------------------------------------------------------------------
// Restart po udanej aktualizacji OTA panelu (tez ESP.restart(), ale nie z
// braku danych) oraz przypadkowe wartosci w pamieci RTC po wlaczeniu
// zasilania - w obu przypadkach centrala ma dostac zwykly powod startu.
void scenariuszRestartPoOta() {
#ifdef DIAG_MA_RATUNEK
  if (!MA_RATUNEK) { sprawdz(false, "firmware ma pamiec autorestartu"); return; }
  memset(&rtcAutorestart, 0xA5, sizeof(rtcAutorestart));   // "smieci" po wlaczeniu zasilania
  g_atrapa.powodResetu = 3;                                // ESP_RST_SW, np. po OTA
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  setup();
  biegnij(3000);
  info("zapytanie, ktore dostala centrala: %s", kopia(0).ostatniStatusSciezka.c_str());
  sprawdz(kopia(0).ostatniStatusSciezka == "/api/status?panel_fw=" WERSJA_FW "&reset_powod=sw", "restart programowy bez znacznika autorestartu: reset_powod=sw, jak dotad");
  sprawdz(!logMa("zdarzenie=start_po_autorestarcie"), "log: brak wpisu start_po_autorestarcie");
  sprawdz(rtcAutorestart.wazna() && rtcAutorestart.oczekuje == 0 && rtcAutorestart.kolejnych == 0 && rtcAutorestart.lacznie == 0,
          "przypadkowe wartosci w pamieci RTC zostaly rozpoznane i wyczyszczone");
#else
  sprawdz(false, "firmware ma pamiec autorestartu");
#endif
}

// --- 17 --------------------------------------------------------------------
// Centrala odpowiada, ale stale nieczytelnie. Ponowne polaczenie WiFi nic tu
// nie da, wiec panel go nie wymusza; restart po 45 s - tak (gdyby przyczyna
// lezala w panelu, np. brak pamieci, restart ja usuwa).
void scenariuszZlaOdpowiedzDlugo() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  setup();
  biegnij(6000);
  size_t od = g_atrapa.serial.size();
  const unsigned long start = millis();
  ustaw(0, [](Centrala& c) { c.trybStatus = TRYB_HTTP500; });
  biegnij(20000);
  info("naglowek po 20 s: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(znacznikNaglowka().rfind("ZLA ODPOWIEDZ", 0) == 0, "naglowek: \"ZLA ODPOWIEDZ <sekundy> s\"");
  sprawdz(logMa("zdarzenie=dane_nieaktualne", "przyczyna=odp", "", od), "log: dane_nieaktualne przyczyna=odp");
  unsigned long restartPo = 0;
  for (int i = 0; i < 300 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  info("ESP.restart() po %lu ms nieczytelnych odpowiedzi", restartPo);
  sprawdz(!logMa("zdarzenie=wymuszam_polaczenie", "", "", od) && g_atrapa.liczbaDisconnect == 0, "WiFi nie jest rozlaczane - siec dziala");
  sprawdz(restartPo >= 42000 && restartPo <= 46500 && logMa("zdarzenie=autorestart", "przyczyna=odp", "", od), "restart po ok. 45 s, przyczyna=odp");
#ifdef DIAG_MA_RATUNEK
  char token[diag::TOKEN_AUTORESTARTU_MAKS] = "";
  if (MA_RATUNEK) diag::tokenAutorestartu(token, sizeof(token), rtcAutorestart);
  info("token, ktory panel zglosi centrali po starcie: %s", token);
  sprawdz(std::string(token) == "auto_odp_r0_n1_t45_h50_b39_eo500", "token dla centrali: przyczyna odp, ostatni blad o500 (kod HTTP 500)");
#else
  sprawdz(false, "token dla centrali: przyczyna odp, ostatni blad o500 (kod HTTP 500)");
#endif
}

// Czy brzeczyk gra wzorzec alarmu: w ciagu 1,2 s czestotliwosc zmienia sie
// i w kazdej chwili jest niezerowa (alarm przegrzania: 1200/1600 Hz bez przerw).
bool brzeczykGraPrzegrzanie() {
  const int przed = g_atrapa.zmianTonu;
  bool caly_czas = true;
  for (int i = 0; i < 12; i++) {
    biegnij(100);
    if (g_atrapa.tonHz != 1200 && g_atrapa.tonHz != 1600) caly_czas = false;
  }
  return caly_czas && g_atrapa.zmianTonu > przed;
}

// --- 17a -------------------------------------------------------------------
// Centrala odpowiada nieczytelnie mimo dwoch samoczynnych restartow z rzedu:
// trzeciego nie ma (nic by nie dal, a kazdy konczylby sie wiadomoscia z
// centrali). Gdy przyczyna sie zmienia (centrala milknie), restart wraca.
void scenariuszZlaOdpowiedzBezKonca() {
#ifdef DIAG_MA_RATUNEK
  if (!MA_RATUNEK) { sprawdz(false, "firmware ma pamiec autorestartu"); return; }
  pamiecPoAutorestarcie(2, diag::PRZYCZYNA_ODPOWIEDZ, 0);
  ustaw(0, [](Centrala& c) { c.trybStatus = TRYB_HTTP500; });
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  setup();
  biegnij(200000);   // prog dla trzeciego restartu to 3 min
  info("naglowek po 200 s: \"%s\"", znacznikNaglowka().c_str());
  info("%s", liniaLogu("zdarzenie=autorestart_pominiety").c_str());
  sprawdz(g_atrapa.restarty == 0, "trzeciego restartu z rzedu przy nieczytelnych odpowiedziach nie ma");
  sprawdz(ileLinii("zdarzenie=autorestart_pominiety") == 1 && logMa("zdarzenie=autorestart_pominiety", "przyczyna=odp", "nr_z_rzedu=2"),
          "log: autorestart_pominiety - raz");
  sprawdz(znacznikNaglowka().rfind("ZLA ODPOWIEDZ ", 0) == 0, "naglowek: \"ZLA ODPOWIEDZ <sekundy> s\", bez odliczania do restartu");
  sprawdz(g_atrapa.liczbaDisconnect == 0, "WiFi nie jest rozlaczane");

  printf("  centrala przestaje odpowiadac w ogole:\n");
  ustaw(0, [](Centrala& c) { c.trybStatus = TRYB_MILCZY; });
  unsigned long restartPo = 0;
  const unsigned long start = millis();
  for (int i = 0; i < 100 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  info("ESP.restart() po %lu ms od zmiany przyczyny", restartPo);
  sprawdz(restartPo > 0 && restartPo <= 5000 && logMa("zdarzenie=autorestart", "przyczyna=milczy", "nr_z_rzedu=3"),
          "przyczyna zmienila sie na \"centrala milczy\": restart nastepuje od razu");
#else
  sprawdz(false, "firmware ma pamiec autorestartu");
#endif
}

// --- 17b -------------------------------------------------------------------
// Przycisk WYCISZ: cisza od razu; wyciszenie konczy sie razem z alarmem
// (v1.2.0: raz wyciszony typ alarmu zostawal niemy az do restartu panelu).
void scenariuszWyciszAlarm() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  ustaw(0, centralaJakWLogu1112);
  setup();
  biegnij(4000);
  sprawdz(ekranMa("PRZEGRZANIE") && brzeczykGraPrzegrzanie(), "alarm przegrzania: ekran alarmu i dzwiek");
  sprawdz(kliknij("WYCISZ"), "przycisk WYCISZ");
  biegnij(50);
  sprawdz(g_atrapa.tonHz == 0, "WYCISZ: cisza od razu");
  biegnij(6000);
  sprawdz(g_atrapa.tonHz == 0, "alarm trwa dalej: cisza zostaje");

  printf("  alarm ustepuje i po chwili wraca:\n");
  ustaw(0, [](Centrala& c) { c.alarmOgrzewanie = false; c.tOgrz = 70.0; });
  biegnij(5000);
  sprawdz(!ekranMa("PRZEGRZANIE") && ekranMa("70°C") && g_atrapa.tonHz == 0, "alarm ustapil: ekran alarmu znika, cisza");
  ustaw(0, [](Centrala& c) { c.alarmOgrzewanie = true; c.tOgrz = 135.0; });
  biegnij(4000);
  sprawdz(ekranMa("PRZEGRZANIE"), "alarm wrocil: ekran alarmu");
  sprawdz(brzeczykGraPrzegrzanie(), "nowe wystapienie alarmu znow gra (wyciszenie skonczylo sie razem z poprzednim)");
}

// --- 18 --------------------------------------------------------------------
// Alarm przegrzania trwa, centrala znika. Do v1.2.0 panel "zamrazal" ostatni
// znany stan alarmu. Samoczynny restart nie moze tego po cichu skasowac:
// stan alarmu (razem z WYCISZ i ZAMKNIJ) ma pojechac przez restart.
void scenariuszAutorestartPrzyAlarmie() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  ustaw(0, centralaJakWLogu1112);
  setup();
  biegnij(4000);
  sprawdz(ekranMa("PRZEGRZANIE"), "alarm przegrzania: ekran alarmu");
  info("ekran alarmu, linia nad przyciskami: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(znacznikNaglowka().rfind("SYMULACJA: ", 0) == 0 && znacznikNaglowka().find("piec") != std::string::npos,
          "ekran alarmu pokazuje, ze temperatura pieca jest symulowana (\"SYMULACJA: ... piec\")");
  zrzutEkranu("1_alarm_z_symulacja");
  sprawdz(brzeczykGraPrzegrzanie(), "brzeczyk gra wzorzec przegrzania (1200/1600 Hz)");
  sprawdz(kliknij("WYCISZ"), "przycisk WYCISZ");
  biegnij(50);
  sprawdz(g_atrapa.tonHz == 0, "WYCISZ: cisza od razu (bez czekania na kolejny status)");
  biegnij(3000);
  sprawdz(g_atrapa.tonHz == 0 && ekranMa("WYCISZONO"), "po kolejnych statusach dalej cisza, przycisk pokazuje WYCISZONO");
  kliknij("WYCISZONO");
  biegnij(3000);
  sprawdz(g_atrapa.tonHz == 0, "drugie nacisniecie tego samego przycisku nie wlacza dzwieku z powrotem");
  sprawdz(kliknij("ZAMKNIJ"), "przycisk ZAMKNIJ");
  biegnij(2500);
  sprawdz(ekranMa("134°C"), "po ZAMKNIJ: dashboard");

  printf("  centrala znika (alarm trwal):\n");
  size_t od = g_atrapa.serial.size();
  g_atrapa.eter.clear();
  unsigned long restartPo = 0;
  const unsigned long start = millis();
  for (int i = 0; i < 600 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  info("%s", liniaLogu("zdarzenie=autorestart", od).c_str());
  sprawdz(restartPo > 0 && restartPo <= 46500, "restart po ok. 45 s bez danych");
#ifdef DIAG_MA_RATUNEK
  info("zapisany stan alarmow: 0x%02X", MA_RATUNEK ? (unsigned)rtcAutorestart.alarmy : 0u);
  sprawdz(MA_RATUNEK && rtcAutorestart.alarmy == diag::spakujAlarmy(false, true, false, true, 2, 1),
          "pamiec RTC: alarm przegrzania pieca, wyciszony (typ 2 = przegrzanie), ekran alarmu zamkniety na poziomie 1");
#else
  sprawdz(false, "pamiec RTC: alarm przegrzania pieca, wyciszony, ekran alarmu zamkniety");
#endif
}

// --- 19 --------------------------------------------------------------------
// Start po samoczynnym restarcie, ktory nastapil W TRAKCIE alarmu (nie
// wyciszonego, ekran alarmu nie zamkniety), centrali nadal nie ma.
void scenariuszStartPoAutorestarcieZAlarmem() {
#ifdef DIAG_MA_RATUNEK
  if (!MA_RATUNEK) { sprawdz(false, "firmware ma pamiec autorestartu"); return; }
  pamiecPoAutorestarcie(1, diag::PRZYCZYNA_WIFI, 201, diag::spakujAlarmy(false, true, false, false, 0, 0));
  const int zmianPrzed = g_atrapa.zmianTonu;
  setup();
  info("setup() trwal %lu ms; brzeczyk w tym czasie zmienil ton %d razy, teraz %u Hz", millis(), g_atrapa.zmianTonu - zmianPrzed, (unsigned)g_atrapa.tonHz);
  sprawdz(g_atrapa.zmianTonu - zmianPrzed >= 20, "brzeczyk gra wzorzec alarmu juz podczas startu (czekanie na WiFi)");
  sprawdz(logMa("tag=PANEL lvl=WARN", "zdarzenie=alarm_sprzed_restartu", "ogrzewanie=1"), "log: alarm_sprzed_restartu");
  biegnij(1000);
  sprawdz(ekranMa("PRZEGRZANIE") && ekranMa("Piec CO powyzej progu") && ekranMa("WYCISZ"), "ekran alarmu wraca po restarcie, mimo braku danych");
  sprawdz(brzeczykGraPrzegrzanie(), "brzeczyk gra dalej w petli glownej");
  info("ekran alarmu, linia nad przyciskami: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(znacznikNaglowka().rfind("BRAK WIFI ", 0) == 0, "ekran alarmu tez mowi, ze panel nie ma danych (\"BRAK WIFI <sekundy> s\")");
  zrzutEkranu("1_alarm_po_restarcie");
  sprawdz(kliknij("WYCISZ"), "przycisk WYCISZ");
  biegnij(50);
  sprawdz(g_atrapa.tonHz == 0, "WYCISZ dziala od razu takze BEZ danych z centrali");
  biegnij(5000);
  sprawdz(g_atrapa.tonHz == 0, "i cisza zostaje");
  sprawdz(kliknij("ZAMKNIJ"), "przycisk ZAMKNIJ");
  biegnij(1500);
  info("naglowek po ZAMKNIJ: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(ileNapisow("--") >= 8 && znacznikNaglowka().rfind("BRAK WIFI", 0) == 0, "po ZAMKNIJ: dashboard z \"--\" i powodem w naglowku");
  zrzutEkranu("2_dashboard_po_zamknij");

  printf("  centrala wraca, alarm juz nieaktywny:\n");
  ustaw(0, [](Centrala& c) { c.tOgrz = 62.0; });
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  biegnij(40000);
  sprawdz(ekranMa("62°C") && !ekranMa("PRZEGRZANIE") && g_atrapa.tonHz == 0, "dane z centrali zastepuja zapamietany stan: kafelki, brak alarmu, cisza");
  sprawdz(g_atrapa.restarty == 0, "kolejnego restartu nie bylo");
#else
  sprawdz(false, "firmware ma pamiec autorestartu");
#endif
}

// --- 23 --------------------------------------------------------------------
// Log panelu z 2026-11-12 (v1.2.0): po starcie kazda odpowiedz centrali
// konczyla sie "json=NoMemory" - panel mial ok. 8 kB wolnego RAM-u i nie mial
// gdzie jej rozpakowac. Na ekranie "--", w logu przyczyna=odp (czyli wina
// centrali), a wina lezala w panelu.
void scenariuszBrakPamieciJson() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -33, 0));
  setup();
  biegnij(6000);
  sprawdz(ekranMa("55°C"), "na poczatku kafelki pokazuja dane centrali");
  const size_t od = g_atrapa.serial.size();
  const unsigned long start = millis();
  // malloc() odmawia blokow od 1000 B: tyle potrzebuje ArduinoJson na pierwsza
  // pule pol odpowiedzi. Male przydzialy (teksty etykiet LVGL) dzialaja dalej.
  pamiec::odmawiajOd = 1000;
  g_atrapa.ramWolny = 2000;
  g_atrapa.ramBlok = 900;
  g_atrapa.ramMin = 700;
  // ...i Serial.printf() tez jej nie dostaje: linie od 64 znakow przepadaja.
  // Wszystko, co sprawdzamy nizej, musi wiec isc przez logBezPamieci().
  g_atrapa.serialBezPamieci = true;
  biegnij(20000);
  info("%s", liniaLogu("zdarzenie=odpowiedz_nieczytelna", od).c_str());
  info("naglowek po 20 s: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(logMa("tag=STATUS lvl=ERR", "zdarzenie=odpowiedz_nieczytelna", "json=NoMemory", od) &&
              logMa("zdarzenie=odpowiedz_nieczytelna", "ram=2000 ram_blok=900", "", od),
          "log: odpowiedz_nieczytelna json=NoMemory z prawdziwa iloscia wolnego RAM-u (ram=2000 ram_blok=900)");
  sprawdz(logMa("tag=PAMIEC lvl=WARN zdarzenie=malo_pamieci ram=2000 ram_blok=900 ram_min=700 prog=16000", "", "", od), "log: ostrzezenie tag=PAMIEC zdarzenie=malo_pamieci");
  sprawdz(ileLinii("zdarzenie=malo_pamieci", "", "", od) == 1, "ostrzezenie o pamieci najwyzej raz na minute");
  sprawdz(ekranMa("--") && !ekranMa("55°C"), "po 10 s kafelki pokazuja \"--\"");
  sprawdz(znacznikNaglowka().rfind("BRAK PAMIECI", 0) == 0, "naglowek: \"BRAK PAMIECI <sekundy> s\" (a nie \"ZLA ODPOWIEDZ\")");
  sprawdz(logMa("zdarzenie=dane_nieaktualne", "przyczyna=pam", "", od), "log: dane_nieaktualne przyczyna=pam");
  unsigned long restartPo = 0;
  for (int i = 0; i < 300 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  info("ESP.restart() po %lu ms bez pamieci na odczytanie odpowiedzi", restartPo);
  info("linii logu utraconych w Serial.printf() z braku pamieci: %d (ostatnia: \"%s...\")", g_atrapa.serialLiniiUtraconych, g_atrapa.serialOstatniaUtracona.c_str());
  sprawdz(logMa("tag=PANEL fw=", "ram=2000 ram_min=700 ram_blok=900", "przyczyna=pam", od), "linia tag=PANEL: ram=2000 ram_min=700 ram_blok=900 przyczyna=pam");
  sprawdz(!logMa("zdarzenie=wymuszam_polaczenie", "", "", od) && g_atrapa.liczbaDisconnect == 0, "WiFi nie jest rozlaczane - siec dziala");
  sprawdz(restartPo >= 42000 && restartPo <= 46500 && logMa("zdarzenie=autorestart", "przyczyna=pam", "", od), "restart po ok. 45 s, przyczyna=pam");
#ifdef DIAG_MA_PAMIEC
  char token[diag::TOKEN_AUTORESTARTU_MAKS] = "";
  diag::tokenAutorestartu(token, sizeof(token), rtcAutorestart);
  info("token, ktory panel zglosi centrali po starcie: %s", token);
  sprawdz(std::string(token) == "auto_pam_r0_n1_t45_h1_b0_em200", "token dla centrali: przyczyna pam, wolny RAM 1 kB, ostatni blad m200");
#else
  sprawdz(false, "token dla centrali: przyczyna pam");
#endif
}

// --- 23b -------------------------------------------------------------------
// To samo o krok wczesniej: pamieci nie starcza juz na ODEBRANIE odpowiedzi
// (bufor String w NonBlockingHttp rosnie w miare odbioru). String z Arduino
// nie zglasza tego - przestaje dopisywac. Przed v1.2.2 odpowiedz byla wtedy
// po cichu ucieta i wygladala jak blad centrali ("zly JSON").
void scenariuszBrakPamieciOdbior() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -33, 0));
  setup();
  biegnij(6000);
  sprawdz(ekranMa("55°C"), "na poczatku kafelki pokazuja dane centrali");
  const size_t od = g_atrapa.serial.size();
  String::odmowaOdDlugosci = 1000;   // odpowiedz /api/status ma ok. 1,8 kB
  g_atrapa.ramWolny = 1500;
  g_atrapa.ramBlok = 800;
  g_atrapa.ramMin = 800;
  biegnij(20000);
  String::odmowaOdDlugosci = 0;
  info("%s", liniaLogu("[STATUS] GET nieudany", od).c_str());
  info("%s", liniaLogu("zdarzenie=odpowiedz_nieczytelna", od).c_str());
  info("naglowek po 20 s: \"%s\"", znacznikNaglowka().c_str());
  sprawdz(logMa("[STATUS] GET nieudany: brak pamieci na odpowiedz errno=12 ENOMEM", "", "", od), "log: \"GET nieudany: brak pamieci na odpowiedz errno=12 ENOMEM\"");
  sprawdz(!logMa("zdarzenie=odpowiedz_nieczytelna", "", "", od), "ucieta odpowiedz nie jest brana za blad centrali (brak linii odpowiedz_nieczytelna)");
  sprawdz(ekranMa("--") && znacznikNaglowka().rfind("BRAK PAMIECI", 0) == 0, "kafelki \"--\", naglowek \"BRAK PAMIECI <sekundy> s\"");
  sprawdz(logMa("zdarzenie=dane_nieaktualne", "przyczyna=pam", "", od), "log: dane_nieaktualne przyczyna=pam");
  sprawdz(!logMa("zdarzenie=wymuszam_polaczenie", "", "", od) && g_atrapa.liczbaDisconnect == 0, "WiFi nie jest rozlaczane - siec dziala");
  const size_t od2 = g_atrapa.serial.size();
  g_atrapa.ramWolny = 52000;
  g_atrapa.ramBlok = 40000;
  biegnij(5000);
  sprawdz(ekranMa("55°C") && znacznikNaglowka().empty() && logMa("zdarzenie=dane_wrocily", "", "", od2), "pamiec wraca: kafelki znow pokazuja dane, naglowek pusty");
}

// --- 24 --------------------------------------------------------------------
// Ten sam log: "task_wdt ... loopTask (CPU 1)", slad wywolan w rysowaniu
// LVGL. LVGL 8 po nieudanym przydziale pamieci staje w petli "while(1);" -
// ekran zamiera na 15 s, potem restart przez watchdog, bez sladu przyczyny.
void scenariuszBrakPamieciLvgl() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -33, 0));
  setup();
  biegnij(6000);
  const size_t od = g_atrapa.serial.size();
  const unsigned long start = millis();
  pamiec::odmawiajOd = 1;   // malloc() odmawia wszystkiego
  g_atrapa.ramWolny = 600;
  g_atrapa.ramBlok = 300;
  g_atrapa.ramMin = 300;
  g_atrapa.napisyTft.clear();
  unsigned long restartPo = 0;
  for (int i = 0; i < 300 && restartPo == 0; i++) {
    biegnij(100);
    if (g_atrapa.restarty > 0) restartPo = millis() - start;
  }
  pamiec::odmawiajOd = 0;
  info("ESP.restart() po %lu ms od chwili, gdy malloc() zaczal odmawiac", restartPo);
  sprawdz(restartPo > 0 && restartPo < 15000 && logMa("tag=PANEL lvl=ERR zdarzenie=lvgl_assert", "", "", od),
          "LVGL nie dostaje pamieci: wpis zdarzenie=lvgl_assert i restart od razu (bez 15 s zamrozenia)");
  sprawdz(logMa("zdarzenie=lvgl_assert ram=600 ram_blok=300 ram_min=300", "", "", od), "log: ile bylo wolnego RAM-u (ram=600 ram_blok=300) - linia wypisana mimo braku pamieci");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog zadania nie zadzialal");
  bool napis = false;
  for (const std::string& t : g_atrapa.napisyTft) if (t == "BRAK PAMIECI - RESTART") napis = true;
  sprawdz(napis, "na ekranie napis \"BRAK PAMIECI - RESTART\"");
#ifdef DIAG_MA_PAMIEC
  char token[diag::TOKEN_AUTORESTARTU_MAKS] = "";
  diag::tokenAutorestartu(token, sizeof(token), rtcAutorestart);
  info("token, ktory panel zglosi centrali po starcie: %s", token);
  const std::string tk = token;
  sprawdz(rtcAutorestart.wazna() && rtcAutorestart.oczekuje == 1 && rtcAutorestart.przyczyna == diag::PRZYCZYNA_LVGL &&
              tk.rfind("auto_lvgl_r0_n1_t", 0) == 0 && tk.size() > 10 && tk.compare(tk.size() - 10, 10, "_h0_b0_ev0") == 0,
          "pamiec RTC: powod dla centrali auto_lvgl_..._h0_b0_ev0");
#else
  sprawdz(false, "pamiec RTC: powod dla centrali auto_lvgl_...");
#endif
}

// --- 25 --------------------------------------------------------------------
// Bufory punktow ekranu Wykresy (18,4 kB) istnieja tylko wtedy, gdy ten ekran
// jest otwarty - niezaleznie od tego, jak zostal zamkniety.
void scenariuszWykresyPamiec() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -33, 0));
  setup();
  biegnij(3000);
  sprawdz(pamiec::zyweBuforyWykresow() == 0, "po starcie buforow wykresow nie ma");
  lv_obj_t* pulpit = lv_scr_act();
  lv_obj_t* drugi = nullptr;
  if (lv_disp_t* d = lv_disp_get_default()) {
    for (uint32_t i = 0; i < d->screen_cnt; i++)
      if (znajdzEtykiete(d->screens[i], "WYKRESY") != nullptr && znajdzEtykiete(d->screens[i], "WYSWIETLACZ") != nullptr) drugi = d->screens[i];
  }
  sprawdz(drugi != nullptr, "drugi pulpit (kafelki WYKRESY i WYSWIETLACZ) istnieje");
  if (drugi == nullptr) return;

  printf("  wejscie na Wykresy i wyjscie przez bezczynnosc:\n");
  { LiczFirmware licz; lv_scr_load(drugi); }
  sprawdz(kliknij("WYKRESY"), "klikniecie kafelka WYKRESY");
  biegnij(500);
  sprawdz(lv_scr_act() != drugi && lv_scr_act() != pulpit && pamiec::zyweBuforyWykresow() == 8, "ekran Wykresy otwarty: 8 buforow punktow");
  biegnij(14500);
  sprawdz(lv_scr_act() == drugi && pamiec::zyweBuforyWykresow() == 0, "po 13,5 s bezczynnosci powrot na drugi pulpit, bufory zwolnione");
  biegnij(14500);
  sprawdz(lv_scr_act() == pulpit, "po kolejnych 13,5 s powrot na pulpit glowny");

  printf("  alarm w trakcie ogladania wykresow:\n");
  { LiczFirmware licz; lv_scr_load(drugi); }
  kliknij("WYKRESY");
  biegnij(500);
  sprawdz(pamiec::zyweBuforyWykresow() == 8, "ekran Wykresy otwarty ponownie: 8 buforow");
  ustaw(0, [](Centrala& c) { c.tOgrz = 134.0; c.alarmOgrzewanie = true; });
  biegnij(3000);
  sprawdz(ekranMa("PRZEGRZANIE") && pamiec::zyweBuforyWykresow() == 0, "ekran alarmu zastapil Wykresy: bufory zwolnione");
  ustaw(0, [](Centrala& c) { c.tOgrz = 55.0; c.alarmOgrzewanie = false; });
  biegnij(3000);

  printf("  wejscie na Wykresy, gdy pamieci na bufory nie ma:\n");
  const size_t od = g_atrapa.serial.size();
  pamiec::odmawiajOd = pamiec::BUFOR_WYKRESU_B;
  pamiec::odmawiajDo = pamiec::BUFOR_WYKRESU_B;
  { LiczFirmware licz; lv_scr_load(drugi); }
  kliknij("WYKRESY");
  biegnij(2000);
  pamiec::odmawiajOd = 0;
  pamiec::odmawiajDo = (size_t)-1;
  sprawdz(lv_scr_act() != drugi && pamiec::zyweBuforyWykresow() == 0 && ileLinii("[WYKRESY] BLAD: brak RAM", "", "", od) == 8,
          "ekran sie otwiera, w logu 8 linii \"[WYKRESY] BLAD: brak RAM\", nic sie nie wywala");
  biegnij(14500);
  sprawdz(lv_scr_act() == drugi && pamiec::zyweBuforyWykresow() == 0, "zamkniecie ekranu bez buforow tez dziala");
  sprawdz(ekranMa("WYKRESY"), "drugi pulpit pokazuje kafelki");
}

// ===========================================================================
// Ekran Wykresy: skad panel bierze dane i co z nich rysuje (v1.3.0)
// ===========================================================================
template <typename F> void ustawC(int idx, F zmiana) {
  std::lock_guard<std::mutex> blokada(g_mutex);
  zmiana(g_centrale[idx]);
}
Centrala stanC(int idx) {
  std::lock_guard<std::mutex> blokada(g_mutex);
  return g_centrale[idx];
}

lv_obj_t* drugiPulpit() {
  if (lv_disp_t* d = lv_disp_get_default()) {
    for (uint32_t i = 0; i < d->screen_cnt; i++)
      if (znajdzEtykiete(d->screens[i], "WYKRESY") != nullptr && znajdzEtykiete(d->screens[i], "WYSWIETLACZ") != nullptr) return d->screens[i];
  }
  return nullptr;
}

// Drugi pulpit -> kafelek WYKRESY. Ekran otwiera sie na podstronie wyboru serii.
bool otworzWykresy() {
  lv_obj_t* drugi = drugiPulpit();
  if (drugi == nullptr) return false;
  { LiczFirmware licz; lv_scr_load(drugi); }
  if (!kliknij("WYKRESY")) return false;
  biegnij(300);
  return lv_scr_act() != drugi && znajdzEtykiete(lv_scr_act(), "POKAZ WYKRES") != nullptr;
}

// Dotkniecie podstrony z wykresem (gdziekolwiek) - firmware wraca wtedy do wyboru serii.
bool dotknijWykresu() {
  lv_obj_t* tytul = znajdzEtykiete(lv_scr_act(), "[");
  if (tytul == nullptr || znajdzEtykiete(lv_scr_act(), "POKAZ WYKRES") != nullptr) return false;
  LiczFirmware licz;
  lv_event_send(lv_obj_get_parent(tytul), LV_EVENT_PRESSED, nullptr);
  return true;
}

lv_obj_t* znajdzWykres(lv_obj_t* o) {
  if (o == nullptr || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return nullptr;
  if (lv_obj_check_type(o, &lv_chart_class)) return o;
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t* w = znajdzWykres(lv_obj_get_child(o, (int32_t)i));
    if (w != nullptr) return w;
  }
  return nullptr;
}
bool wykresWidoczny() { return znajdzWykres(lv_scr_act()) != nullptr; }

// To, co lv_chart ma narysowac: wartosc x10 (21,5 C -> 215) albo BRAK (linia
// przerwana w tym miejscu). `slot`: 0..6 = serie lewej osi w kolejnosci
// wyboru, 7 = seria prawej osi. Pusty wynik = wykresu nie ma na ekranie.
const int BRAK = INT_MIN;
std::vector<int> punktyWykresu(int slot) {
  std::vector<int> wynik;
  lv_obj_t* wykres = znajdzWykres(lv_scr_act());
  if (wykres == nullptr) return wynik;
  // LVGL 8.3 trzyma serie od ostatnio dodanej do pierwszej.
  std::vector<lv_chart_series_t*> serie;
  for (lv_chart_series_t* s = lv_chart_get_series_next(wykres, nullptr); s != nullptr; s = lv_chart_get_series_next(wykres, s)) serie.insert(serie.begin(), s);
  if (slot < 0 || slot >= (int)serie.size()) return wynik;
  const lv_coord_t* tab = lv_chart_get_y_array(wykres, serie[(size_t)slot]);
  const uint16_t n = lv_chart_get_point_count(wykres);
  for (uint16_t i = 0; i < n; i++) wynik.push_back(tab[i] == LV_CHART_POINT_NONE ? BRAK : (int)tab[i]);
  return wynik;
}
int ilePunktow(const std::vector<int>& p) {
  int n = 0;
  for (int v : p) if (v != BRAK) n++;
  return n;
}
std::string punktyTekst(const std::vector<int>& p) {
  std::string s;
  for (size_t i = 0; i < p.size(); i++) s += (i ? " " : "") + (p[i] == BRAK ? std::string("-") : std::to_string(p[i]));
  return s;
}

// Porownanie tego, co jest na wykresie, z tym, co ma byc. Przy roznicy wypisuje pierwsza.
bool tesamePunkty(const std::vector<int>& jest, const std::vector<int>& maByc, const char* co) {
  if (jest.size() != maByc.size()) {
    info("%s: na wykresie %zu pozycji, mialo byc %zu", co, jest.size(), maByc.size());
    return false;
  }
  for (size_t i = 0; i < jest.size(); i++) {
    if (jest[i] == maByc[i]) continue;
    const std::string a = jest[i] == BRAK ? "brak punktu" : std::to_string(jest[i]);
    const std::string b = maByc[i] == BRAK ? "brak punktu" : std::to_string(maByc[i]);
    info("%s: pozycja %zu: na wykresie %s, mialo byc %s", co, i, a.c_str(), b.c_str());
    return false;
  }
  return true;
}

std::vector<std::string> podzielTekst(const std::string& s, char znak) {
  std::vector<std::string> w;
  size_t p = 0;
  while (true) {
    const size_t k = s.find(znak, p);
    if (k == std::string::npos) { w.push_back(s.substr(p)); break; }
    w.push_back(s.substr(p, k - p));
    p = k + 1;
  }
  return w;
}

// Co panel MA narysowac z odpowiedzi centrali 3.58.4 (plik wzorca) dla
// kolumny `kolumna` (numeracja jak w /api/history/long: 1 = ZEWN srednia,
// 4 = BOJLER, 19 = pokoj, 22 = cisnienie, 31 = syberek). Liczone tutaj,
// niezaleznie od firmware'u: linia "#przerwa" i pole "nan" = brak punktu,
// reszta = liczba x10. Zawsze 96 pozycji; te bez danych to brak punktu.
std::vector<int> zWzorca(const std::string& tresc, int kolumna) {
  std::vector<int> w;
  for (const std::string& linia : podzielTekst(tresc, '\n')) {
    if (linia.empty() || linia.rfind("#panel-hist", 0) == 0 || linia.rfind("#koniec", 0) == 0) continue;
    if (linia == "#przerwa") { w.push_back(BRAK); continue; }
    const std::vector<std::string> kol = podzielTekst(linia, ',');
    if (kolumna >= (int)kol.size() || kol[(size_t)kolumna] == "nan") { w.push_back(BRAK); continue; }
    w.push_back((int)lround(atof(kol[(size_t)kolumna].c_str()) * 10.0));
  }
  while (w.size() < 96) w.push_back(BRAK);
  return w;
}
int ilePrzerwWeWzorcu(const std::string& tresc) {
  int n = 0;
  for (const std::string& linia : podzielTekst(tresc, '\n')) if (linia == "#przerwa") n++;
  return n;
}

// Wartosc pola "klucz=" z ostatniej linii logu zawierajacej `fragment` (od pozycji `od`); -1 = nie ma.
long poleLogu(const std::string& fragment, const char* klucz, size_t od) {
  const std::string& log = g_atrapa.serial;
  const size_t p = log.rfind(fragment);
  if (p == std::string::npos || p < od) return -1;
  size_t k = log.find('\n', p);
  if (k == std::string::npos) k = log.size();
  const std::string linia = log.substr(p, k - p);
  const size_t q = linia.find(std::string(" ") + klucz + "=");
  if (q == std::string::npos) return -1;
  return atol(linia.c_str() + q + strlen(klucz) + 2);
}

// Punkty, ktore z TEGO SAMEGO starego CSV narysowal firmware v1.2.2 (plik
// wzorce_v122/punkty.txt; jak powstal: CZYTAJ.txt obok niego).
std::vector<int> zWzorcaV122(const char* nazwa) {
  const char* katalog = getenv("LCD_WZORCE_V122");
  const std::string sciezka = std::string(katalog != nullptr ? katalog : "wzorce_v122") + "/punkty.txt";
  FILE* f = fopen(sciezka.c_str(), "rb");
  std::vector<int> w;
  if (f == nullptr) {
    fprintf(stderr, "brak pliku %s (zmienna LCD_WZORCE_V122 ustawia katalog; robi to uruchom_testy.sh)\n", sciezka.c_str());
    exit(2);
  }
  std::string calosc;
  char b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof(b), f)) > 0) calosc.append(b, n);
  fclose(f);
  const std::string poczatek = std::string(nazwa) + ":";
  for (const std::string& linia : podzielTekst(calosc, '\n')) {
    if (linia.rfind(poczatek, 0) != 0) continue;
    for (const std::string& pole : podzielTekst(linia.substr(poczatek.size()), ' ')) {
      if (pole.empty()) continue;
      w.push_back(pole == "-" ? BRAK : atoi(pole.c_str()));
    }
    return w;
  }
  fprintf(stderr, "w pliku %s nie ma wpisu \"%s\"\n", sciezka.c_str(), nazwa);
  exit(2);
}

// Jedno "pokaz wykres": zakres, klikniecie, czekanie. Zwraca pozycje w logu sprzed klikniecia.
size_t pokazWykres(const char* zakres, unsigned long czekajMs) {
  if (zakres != nullptr) kliknij(zakres);
  const size_t od = g_atrapa.serial.size();
  kliknij("POKAZ WYKRES");
  biegnij(czekajMs);
  return od;
}

// --- 26 --------------------------------------------------------------------
// Centrala 3.58.4 + panel 1.3.0: po dotknieciu "POKAZ WYKRES" panel pyta
// centrale o gotowe punkty, a centrala bierze je ze swojej historii.
// Odpowiedzi centrali w tym scenariuszu to pliki zapisane JEJ kodem
// (wzorce_centrali/); to, co ma byc na wykresie, test liczy z tych plikow sam.
void scenariuszWykresyCentralaLiczy() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  const std::string dzien = wzorzec("dzien.txt"), godz = wzorzec("godz_ta_sama_doba.txt"), tydz = wzorzec("tydz_pelne_bufory.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = dzien; c.panelHist["godz"] = godz; c.panelHist["tydz"] = tydz; });
  setup();
  biegnij(3000);
  sprawdz(otworzWykresy(), "drugi pulpit -> kafelek WYKRESY: otwiera sie wybor serii");
  sprawdz(kliknij("ZEWN") && kliknij("BOJLER"), "wybrane serie: ZEWN i BOJLER");

  printf("  zakres DZIENNY (domyslny):\n");
  const int statusPrzed = stanC(0).nStatus;
  size_t od = g_atrapa.serial.size();
  const unsigned long t0 = millis();
  sprawdz(kliknij("POKAZ WYKRES"), "dotkniecie POKAZ WYKRES");
  sprawdz(millis() == t0 && ekranMa("Ladowanie danych z centrali...") && !wykresWidoczny(),
          "dotkniecie wraca od razu (zegar plytki nie drgnal), na ekranie \"Ladowanie danych z centrali...\"");
  biegnij(1000);
  Centrala c = stanC(0);
  sprawdz(c.nPanelHist == 1 && c.ostatniaPanelHistSciezka == "/api/history/panel?tryb=dzien&pkt=96",
          "centrala dostala jedno zapytanie: GET /api/history/panel?tryb=dzien&pkt=96");
  sprawdz(c.nHistKrotka == 0 && c.nHistDluga == 0, "calego CSV historii (/api/history, /api/history/long) panel NIE pobieral");
  sprawdz(wykresWidoczny() && !ekranMa("Ladowanie danych z centrali..."), "wykres jest na ekranie");
  sprawdz(tesamePunkty(punktyWykresu(0), zWzorca(dzien, 1), "ZEWN"), "ZEWN: wszystkie 96 punktow takie, jak policzyla centrala (od -4,9 do 9,3)");
  sprawdz(tesamePunkty(punktyWykresu(1), zWzorca(dzien, 4), "BOJLER"), "BOJLER: wszystkie 96 punktow (45,0)");
  sprawdz(ilePunktow(punktyWykresu(2)) == 0 && ilePunktow(punktyWykresu(7)) == 0, "pozostale serie puste");
  sprawdz(ekranMa("12.11 13:01  -  13.11 13:00"), "podpis pod wykresem: \"12.11 13:01  -  13.11 13:00\" (zakres podany przez centrale, z data)");
  sprawdz(ekranMa("[Dzienny] ZEWN + BOJLER"), "tytul: \"[Dzienny] ZEWN + BOJLER\"");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=dzien sposob=centrala punktow=96 przerw=0 wierszy_centrali=1440", "ucietych_linii=0", "", od),
          "log: zdarzenie=pobrano tryb=dzien sposob=centrala punktow=96 przerw=0 wierszy_centrali=1440 ... ucietych_linii=0");
  info("%s", liniaLogu("tag=WYKRESY zdarzenie=pobrano", od).c_str());
  info("czas w symulatorze: %ld ms zegara plytki, %ld B - to NIE jest pomiar na plytce (tu siec jest lokalna)",
       poleLogu("tag=WYKRESY zdarzenie=pobrano", "ms", od), poleLogu("tag=WYKRESY zdarzenie=pobrano", "bajtow", od));
  sprawdz(!logMa("tag=WYKRESY", "zdarzenie=blad", "", od) && !logMa("zdarzenie=stara_centrala", "", "", od), "w logu zadnego bledu ani przejscia na stary sposob");
  zrzutEkranu("dzien");

  printf("  powrot do wyboru i zakres GODZINNY:\n");
  sprawdz(dotknijWykresu() && ekranMa("POKAZ WYKRES"), "dotkniecie wykresu wraca do wyboru serii");
  od = pokazWykres("GODZINNY", 1000);
  c = stanC(0);
  sprawdz(c.nPanelHist == 2 && c.ostatniaPanelHistSciezka == "/api/history/panel?tryb=godz&pkt=96", "zapytanie: /api/history/panel?tryb=godz&pkt=96");
  sprawdz(tesamePunkty(punktyWykresu(0), zWzorca(godz, 1), "ZEWN") && tesamePunkty(punktyWykresu(1), zWzorca(godz, 4), "BOJLER"),
          "ZEWN i BOJLER: 96 punktow z ostatniej godziny (stary sposob dawal 60 punktow na 62% szerokosci)");
  sprawdz(ekranMa("12:00  -  13:00") && ekranMa("[Godzinny] ZEWN + BOJLER"), "podpis \"12:00  -  13:00\", tytul \"[Godzinny] ZEWN + BOJLER\"");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=godz sposob=centrala punktow=96", "", "", od), "log: zdarzenie=pobrano tryb=godz sposob=centrala punktow=96");

  printf("  zakres TYGODNIOWY, pelne bufory centrali, najdluzsze liczby; do tego CISNIENIE na prawej osi:\n");
  dotknijWykresu();
  sprawdz(kliknij("CISNIENIE"), "dobrana seria CISNIENIE (inna jednostka -> prawa os)");
  od = pokazWykres("TYGODNIOWY", 1000);
  c = stanC(0);
  sprawdz(c.nPanelHist == 3 && c.ostatniaPanelHistSciezka == "/api/history/panel?tryb=tydz&pkt=96", "zapytanie: /api/history/panel?tryb=tydz&pkt=96");
  sprawdz(tesamePunkty(punktyWykresu(0), zWzorca(tydz, 1), "ZEWN") && tesamePunkty(punktyWykresu(1), zWzorca(tydz, 4), "BOJLER"),
          "ZEWN (wartosci ujemne) i BOJLER: 96 punktow jak w odpowiedzi centrali");
  sprawdz(tesamePunkty(punktyWykresu(7), zWzorca(tydz, 22), "CISNIENIE"), "CISNIENIE (1013,2 hPa) na prawej osi: 96 punktow");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=tydz sposob=centrala punktow=96 przerw=0 wierszy_centrali=9440", "ucietych_linii=0", "", od),
          "log: wierszy_centrali=9440 (8000 zdarzen + 1440 probek), ucietych_linii=0");
  info("odpowiedz centrali: %ld B (caly /api/history/long przy pelnym buforze to ponad 1 MB)", poleLogu("tag=WYKRESY zdarzenie=pobrano", "bajtow", od));

  printf("  ostatnia kolumna wiersza (Syberek) - czy dlugie linie dochodza cale:\n");
  dotknijWykresu();
  kliknij("CISNIENIE");   // odznacz
  sprawdz(kliknij("Syberek"), "CISNIENIE odznaczone, wybrany Syberek");
  od = pokazWykres(nullptr, 1000);
  sprawdz(tesamePunkty(punktyWykresu(7), zWzorca(tydz, 31), "Syberek"), "Syberek: 96 punktow (60,0, na koncu 100,0)");

  c = stanC(0);
  sprawdz(c.nPanelHist == 4 && c.nHistKrotka == 0 && c.nHistDluga == 0 && c.nHistWyslanych == 4 && c.nHistPorzuconych == 0,
          "razem 4 zapytania o gotowe punkty, 4 odpowiedzi odebrane do konca, ani jednego pobrania calego CSV");
  info("zapytan /api/status w tym czasie (ok. 4,5 s): %d", c.nStatus - statusPrzed);
  sprawdz(c.nStatus >= statusPrzed + 2, "w tym czasie panel dalej odpytywal /api/status (co 2 s)");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal");
}

// --- 27 --------------------------------------------------------------------
// Panel 1.3.0 ze STARSZA centrala (<= 3.58.3), ktora nie zna
// /api/history/panel: panel ma to rozpoznac i narysowac wykres po staremu -
// dokladnie taki, jaki z tego samego CSV rysowal v1.2.2 (wzorce_v122/).
void scenariuszWykresyStaraCentrala() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  ustawC(0, [](Centrala& c) { c.wersjaHistorii = HIST_302; });
  setup();
  biegnij(3000);
  sprawdz(otworzWykresy(), "ekran Wykresy otwarty");
  sprawdz(kliknij("ZEWN"), "wybrana seria ZEWN");

  printf("  zakres DZIENNY:\n");
  size_t od = pokazWykres(nullptr, 3000);
  Centrala c = stanC(0);
  sprawdz(c.nPanelHist == 1 && c.nHistKrotka == 1 && c.nHistDluga == 0,
          "panel najpierw zapytal o /api/history/panel (odpowiedz 302), potem pobral /api/history");
  sprawdz(logMa("tag=WYKRESY zdarzenie=stara_centrala kod=302", "", "", od), "log: zdarzenie=stara_centrala kod=302");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=dzien sposob=panel", "linii=1440", "", od), "log: zdarzenie=pobrano tryb=dzien sposob=panel ... linii=1440");
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorcaV122("dzien_zewn"), "ZEWN"),
          "wykres punkt w punkt taki, jaki z tego CSV rysowal v1.2.2");
  info("%s", liniaLogu("tag=WYKRESY zdarzenie=pobrano", od).c_str());

  printf("  zakres GODZINNY:\n");
  dotknijWykresu();
  od = pokazWykres("GODZINNY", 3000);
  c = stanC(0);
  sprawdz(c.nPanelHist == 2 && c.nHistKrotka == 2, "znowu: /api/history/panel (302), potem /api/history");
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorcaV122("godz_zewn"), "ZEWN"), "wykres taki sam jak w v1.2.2 (ostatnie 60 wierszy)");

  printf("  zakres TYGODNIOWY (2000 wierszy /api/history/long, 352 kB):\n");
  dotknijWykresu();
  od = pokazWykres("TYGODNIOWY", 5000);
  c = stanC(0);
  sprawdz(c.nPanelHist == 3 && c.nHistDluga == 1, "/api/history/panel (302), potem /api/history/long");
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorcaV122("tydz_zewn"), "ZEWN"), "wykres taki sam jak w v1.2.2");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=tydz sposob=panel", "linii=2000", "", od), "log: zdarzenie=pobrano tryb=tydz sposob=panel ... linii=2000");
  info("%s", liniaLogu("tag=WYKRESY zdarzenie=pobrano", od).c_str());

  printf("  przerwa w danych (centrala nie pracowala 30 minut):\n");
  ustawC(0, [](Centrala& x) { x.histDziuraOd = 700; x.histDziuraIle = 30; });
  dotknijWykresu();
  od = pokazWykres("DZIENNY", 3000);
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorcaV122("dzien_przerwa_zewn"), "ZEWN"),
          "wykres z przerwa taki sam jak w v1.2.2 (linia przerwana w tym samym miejscu)");
  ustawC(0, [](Centrala& x) { x.histDziuraOd = -1; x.histDziuraIle = 0; });

  printf("  polaczenie zerwane w srodku 51. wiersza starego CSV:\n");
  {
    // Urywamy zaraz po "HH:MM,-" 51. wiersza. Gdyby panel potraktowal ten
    // ogryzek jak wiersz, dorysowalby 51. punkt o wartosci 0.
    const std::string csv = csvKrotki(1440, -1, 0);
    size_t poczatek51 = 0;
    for (int i = 0; i < 50; i++) poczatek51 = csv.find('\n', poczatek51) + 1;
    const long urwij = (long)poczatek51 + 7;
    std::vector<int> maByc;
    for (const std::string& linia : podzielTekst(csv.substr(0, poczatek51), '\n'))
      if (!linia.empty()) maByc.push_back((int)lround(atof(podzielTekst(linia, ',')[1].c_str()) * 10.0));
    while (maByc.size() < 96) maByc.push_back(BRAK);
    sprawdz(csv.compare(poczatek51 + 5, 2, ",-") == 0, "(test) 51. wiersz zaczyna sie od \"HH:MM,-\"");
    ustawC(0, [urwij](Centrala& x) { x.histUrwijPo = urwij; });
    dotknijWykresu();
    od = pokazWykres("DZIENNY", 3000);
    sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), maByc, "ZEWN"), "wykres z 50 calych wierszy; urwany 51. wiersz nie jest danymi");
    bool dopisek = false;
    for (const std::string& t : napisyEkranu()) if (t.find("   NIEPELNE") != std::string::npos) dopisek = true;
    sprawdz(dopisek, "podpis pod wykresem z dopiskiem NIEPELNE");
    sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=pobrano_czesc tryb=dzien sposob=panel powod=urwana", "punktow=50 linii=50 ", "", od),
            "log: zdarzenie=pobrano_czesc powod=urwana ... punktow=50 linii=50");
    info("%s", liniaLogu("tag=WYKRESY lvl=WARN zdarzenie=pobrano_czesc", od).c_str());
    ustawC(0, [](Centrala& x) { x.histUrwijPo = -1; });
  }

  printf("  bardzo stara centrala: pod /api/history/panel oddaje caly krotki CSV z kodem 200:\n");
  ustawC(0, [](Centrala& x) { x.wersjaHistorii = HIST_PRZEDROSTEK; });
  dotknijWykresu();
  const Centrala przed = stanC(0);
  od = pokazWykres("TYGODNIOWY", 5000);
  c = stanC(0);
  sprawdz(logMa("tag=WYKRESY zdarzenie=stara_centrala kod=200", "", "", od), "log: zdarzenie=stara_centrala kod=200 (pierwsza linia to nie \"#panel-hist\")");
  sprawdz(c.nPanelHist == przed.nPanelHist + 1 && c.nHistDluga == przed.nHistDluga + 1, "panel nie czytal tej odpowiedzi dalej, tylko pobral /api/history/long");
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorcaV122("tydz_zewn"), "ZEWN"), "wykres tygodniowy taki sam jak w v1.2.2");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal");
}

// --- 28 --------------------------------------------------------------------
// Przerwy w pracy centrali i czujnik bez odczytu: linia ma byc przerwana tam,
// gdzie danych nie bylo, a nie poprowadzona "na skroty".
void scenariuszWykresyPrzerwy() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  const std::string tydz = wzorzec("tydz_przerwa.txt"), godz = wzorzec("godz_bez_czujnika.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["tydz"] = tydz; c.panelHist["godz"] = godz; });
  setup();
  biegnij(3000);
  otworzWykresy();
  kliknij("ZEWN");

  printf("  tydzien, w ktorym centrala byla wylaczona przez 1,5 doby:\n");
  size_t od = pokazWykres("TYGODNIOWY", 1000);
  const int przerw = ilePrzerwWeWzorcu(tydz);
  const std::vector<int> p = punktyWykresu(0);
  sprawdz(przerw == 19 && wykresWidoczny() && tesamePunkty(p, zWzorca(tydz, 1), "ZEWN"),
          "96 pozycji: 77 punktow i 19 przerw, dokladnie w miejscach podanych przez centrale");
  sprawdz(p.size() == 96 && p[41] != BRAK && p[42] == BRAK && p[60] == BRAK && p[61] != BRAK,
          "przerwa to 19 kolejnych pozycji (43. do 61. z 96) - os czasu jest rowna, 1,5 doby ma na niej swoja szerokosc");
  sprawdz(ekranMa("06.11 13:00  -  13.11 13:00"), "podpis: \"06.11 13:00  -  13.11 13:00\" (pelne 7 dni)");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=tydz sposob=centrala punktow=77 przerw=19", "", "", od), "log: punktow=77 przerw=19");
  zrzutEkranu("tydzien_z_przerwa");

  printf("  godzina, w ktorej czujnik pokojowy przez pierwsze pol godziny nie dawal odczytu:\n");
  dotknijWykresu();
  sprawdz(kliknij("Wewnetrzna"), "dobrana seria Wewnetrzna (pokoj)");
  od = pokazWykres("GODZINNY", 1000);
  const std::vector<int> zewn = punktyWykresu(0), pokoj = punktyWykresu(1);
  sprawdz(tesamePunkty(zewn, zWzorca(godz, 1), "ZEWN") && ilePunktow(zewn) == 96, "ZEWN: pelne 96 punktow (3,0)");
  sprawdz(tesamePunkty(pokoj, zWzorca(godz, 19), "Wewnetrzna"), "Wewnetrzna: brak punktow tam, gdzie centrala podala \"nan\", potem 21,5");
  sprawdz(pokoj.size() == 96 && pokoj[0] == BRAK && pokoj[40] == BRAK && pokoj[95] == 215 && ilePunktow(pokoj) > 40 && ilePunktow(pokoj) < 56,
          "linia czujnika zaczyna sie w polowie wykresu - nie jest dociagnieta do zera ani do poprzedniej wartosci");
  lv_obj_t* wykres = znajdzWykres(lv_scr_act());
  const lv_chart_t* dane = (const lv_chart_t*)wykres;
  info("zakres lewej osi: %d .. %d (x0,1 C)", wykres ? (int)dane->ymin[0] : 0, wykres ? (int)dane->ymax[0] : 0);
  sprawdz(wykres != nullptr && dane->ymin[0] >= 10 && dane->ymin[0] <= 13 && dane->ymax[0] >= 232 && dane->ymax[0] <= 235,
          "zakres osi policzony z prawdziwych wartosci (3,0..21,5 + margines), brak odczytu go nie psuje");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=godz sposob=centrala punktow=96 przerw=0", "", "", od), "log: punktow=96 przerw=0");
  zrzutEkranu("czujnik_bez_odczytu");
}

// --- 29 --------------------------------------------------------------------
// Centrala odpowiada poprawnie, ale nie ma z czego zrobic wykresu - albo nie
// ma na to pamieci. Kazdy przypadek ma wlasny napis (w v1.2.2 wszystko
// konczylo sie na "Brak danych (WiFi/centrala niedostepna)").
void scenariuszWykresyBrakDanych() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  const std::string pusta = wzorzec("dzien_pusta.txt"), jedna = wzorzec("godz_jedna_probka.txt"), bezZegara = wzorzec("dzien_bez_zegara.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = pusta; c.panelHist["godz"] = jedna; c.panelHist["tydz"] = pusta; });
  setup();
  biegnij(3000);
  otworzWykresy();
  kliknij("ZEWN");

  printf("  centrala swiezo po pierwszym uruchomieniu (historia pusta):\n");
  size_t od = pokazWykres("DZIENNY", 1000);
  sprawdz(!wykresWidoczny() && ekranMa("Centrala nie ma jeszcze danych z tego zakresu\n(pierwszy pomiar zapisuje minute po starcie)"),
          "zamiast wykresu: \"Centrala nie ma jeszcze danych z tego zakresu (pierwszy pomiar zapisuje minute po starcie)\"");
  sprawdz(logMa("tag=WYKRESY zdarzenie=brak_danych tryb=dzien sposob=centrala przyczyna=pusto", "", "", od) && !logMa("zdarzenie=blad", "", "", od),
          "log: zdarzenie=brak_danych przyczyna=pusto (to nie blad)");
  sprawdz(stanC(0).nHistKrotka == 0, "panel nie probuje wtedy starego sposobu");

  printf("  to samo, gdy centrala ma wylaczony zapis historii na flash (w naglowku odpowiedzi fs=0):\n");
  const std::string pustaBezFs = wzorzec("dzien_pusta_bez_fs.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = pustaBezFs; });
  dotknijWykresu();
  od = pokazWykres(nullptr, 1000);
  sprawdz(!wykresWidoczny() && ekranMa("Centrala nie ma jeszcze danych z tego zakresu\n(pierwszy pomiar zapisuje minute po starcie)\n\n"
                                       "Zapis historii na flash jest w centrali wylaczony:\npo jej restarcie historia zaczyna sie od zera"),
          "napis dopowiada: \"Zapis historii na flash jest w centrali wylaczony: po jej restarcie historia zaczyna sie od zera\"");
  zrzutEkranu("zapis_wylaczony");   // najdluzszy napis tego ekranu - widac, czy sie miesci
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = pusta; });

  printf("  jeden pomiar:\n");
  dotknijWykresu();
  od = pokazWykres("GODZINNY", 1000);
  sprawdz(!wykresWidoczny() && ekranMa("Za malo danych na wykres (jeden pomiar)\nSprobuj za kilka minut"), "napis: \"Za malo danych na wykres (jeden pomiar) / Sprobuj za kilka minut\"");
  sprawdz(logMa("tag=WYKRESY zdarzenie=brak_danych tryb=godz sposob=centrala przyczyna=za_malo", "", "", od), "log: zdarzenie=brak_danych przyczyna=za_malo");

  printf("  centrala bez ustawionego zegara (30 pomiarow z czasem 0):\n");
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = bezZegara; });
  dotknijWykresu();
  od = pokazWykres("DZIENNY", 1000);
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorca(bezZegara, 1), "ZEWN"), "wykres jest: 96 punktow, wartosci rosna od 0,0");
  sprawdz(ekranMa("czas nieznany (zegar centrali nieustawiony)"), "podpis: \"czas nieznany (zegar centrali nieustawiony)\"");
  zrzutEkranu("bez_zegara");

  printf("  centrali zabraklo pamieci na odpowiedz (HTTP 503):\n");
  ustawC(0, [](Centrala& c) { c.panelHistKod = 503; });
  dotknijWykresu();
  od = pokazWykres(nullptr, 1000);
  sprawdz(!wykresWidoczny() && ekranMa("Centrali zabraklo pamieci na wykres (HTTP 503)\nSprobuj ponownie"), "napis: \"Centrali zabraklo pamieci na wykres (HTTP 503) / Sprobuj ponownie\"");
  sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=dzien sposob=centrala przyczyna=http kod=503", "", "", od), "log: zdarzenie=blad przyczyna=http kod=503");
  sprawdz(stanC(0).nHistKrotka == 0 && stanC(0).nHistDluga == 0, "panel NIE siega wtedy po caly CSV (centrala zna adres, tylko nie ma pamieci)");
  ustawC(0, [](Centrala& c) { c.panelHistKod = 200; });
  dotknijWykresu();
  od = pokazWykres(nullptr, 1000);
  sprawdz(wykresWidoczny(), "ponowna proba, gdy centrala ma juz pamiec: wykres");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal");
}

// --- 30 --------------------------------------------------------------------
// Odpowiedz centrali nie dochodzi cala albo wcale. Panel ma to nazwac po
// imieniu, nie rysowac wykresu z kawalka i ani na chwile nie przestac
// obslugiwac ekranu i /api/status.
void scenariuszWykresyBledySieci() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  const std::string dzien = wzorzec("dzien.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = dzien; });
  setup();
  biegnij(3000);
  otworzWykresy();
  kliknij("ZEWN");

  printf("  polaczenie zerwane w polowie odpowiedzi (przyszlo 9000 z 18207 B):\n");
  ustawC(0, [](Centrala& c) { c.histUrwijPo = 9000; });
  size_t od = pokazWykres(nullptr, 1000);
  sprawdz(!wykresWidoczny() && ekranMa("Odpowiedz centrali urwala sie w polowie\nSprobuj ponownie"), "napis: \"Odpowiedz centrali urwala sie w polowie / Sprobuj ponownie\" - wykresu z kawalka nie ma");
  sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=dzien sposob=centrala przyczyna=urwana kod=200", "", "", od), "log: zdarzenie=blad przyczyna=urwana kod=200");
  info("%s", liniaLogu("tag=WYKRESY lvl=WARN zdarzenie=blad", od).c_str());

  printf("  to samo, gdy centrala nie podaje dlugosci odpowiedzi (brak Content-Length):\n");
  ustawC(0, [](Centrala& c) { c.histBezDlugosci = true; });
  dotknijWykresu();
  od = pokazWykres(nullptr, 1000);
  sprawdz(!wykresWidoczny() && ekranMa("Odpowiedz centrali urwala sie w polowie\nSprobuj ponownie"), "tez rozpoznane (brakuje linii \"#koniec\")");
  ustawC(0, [](Centrala& c) { c.histUrwijPo = -1; });
  dotknijWykresu();
  od = pokazWykres(nullptr, 1000);
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorca(dzien, 1), "ZEWN"), "cala odpowiedz bez Content-Length: wykres, 96 punktow");
  ustawC(0, [](Centrala& c) { c.histBezDlugosci = false; c.histCrlf = true; });
  dotknijWykresu();
  od = pokazWykres(nullptr, 1000);
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorca(dzien, 1), "ZEWN"), "linie zakonczone CR LF: wykres, 96 punktow");
  ustawC(0, [](Centrala& c) { c.histCrlf = false; });

  printf("  centrala milknie w polowie odpowiedzi (polaczenie otwarte, dane nie plyna):\n");
  ustawC(0, [](Centrala& c) { c.histZamilknijPo = 9000; });
  dotknijWykresu();
  int statusPrzed = stanC(0).nStatus;
  od = pokazWykres(nullptr, 3000);
  const lv_obj_t* postep = znajdzEtykiete(lv_scr_act(), "Ladowanie danych z centrali...\n");
  const std::string postepTekst = postep ? std::string(lv_label_get_text(postep)).substr(31) : "(brak napisu)";
  info("druga linia napisu: \"%s\"", postepTekst.c_str());
  sprawdz(!wykresWidoczny() && postepTekst.find("wierszy (9 kB)") != std::string::npos, "po 3 s pod \"Ladowanie danych z centrali...\" widac, ile doszlo (9 kB)");
  zrzutEkranu("w_trakcie_pobierania");
  biegnij(4000);
  sprawdz(!wykresWidoczny() && ekranMa("Centrala nie odpowiedziala w czasie\nSprobuj ponownie"), "po 6 s ciszy: \"Centrala nie odpowiedziala w czasie / Sprobuj ponownie\"");
  sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=dzien sposob=centrala przyczyna=limit_czasu kod=200 etap=p", "", "", od), "log: przyczyna=limit_czasu etap=p");
  sprawdz(stanC(0).nStatus >= statusPrzed + 3, "przez te 7 s panel dalej odpytywal /api/status (w v1.2.2 petla stala)");
  ustawC(0, [](Centrala& c) { c.histZamilknijPo = -1; });

  printf("  centrala przyjmuje zapytanie i nie odpowiada wcale:\n");
  ustawC(0, [](Centrala& c) { c.histZwlokaMs = 60000; });
  dotknijWykresu();
  statusPrzed = stanC(0).nStatus;
  od = pokazWykres(nullptr, 7000);
  sprawdz(!wykresWidoczny() && ekranMa("Centrala nie odpowiedziala w czasie\nSprobuj ponownie"), "po 6 s: \"Centrala nie odpowiedziala w czasie / Sprobuj ponownie\"");
  sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=dzien sposob=centrala przyczyna=limit_czasu kod=0 etap=h", "", "", od), "log: przyczyna=limit_czasu kod=0 etap=h");
  sprawdz(stanC(0).nStatus >= statusPrzed + 3, "/api/status odpytywany przez caly ten czas");
  ustawC(0, [](Centrala& c) { c.histZwlokaMs = 0; });

  printf("  pod adresami historii odpowiada cos, co nie jest centrala pieca (strona HTML):\n");
  ustawC(0, [](Centrala& c) { c.histHtml = true; });
  dotknijWykresu();
  od = pokazWykres(nullptr, 2000);
  sprawdz(!wykresWidoczny() && ekranMa("Centrala odpowiedziala czyms innym niz dane wykresu"), "napis: \"Centrala odpowiedziala czyms innym niz dane wykresu\" (v1.2.2 rysowal z takiej strony wykres - patrz wykresy_stary_csv)");
  zrzutEkranu("zla_odpowiedz");
  sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=dzien sposob=panel przyczyna=zla_odpowiedz", "", "", od), "log: przyczyna=zla_odpowiedz");
  ustawC(0, [](Centrala& c) { c.histHtml = false; });

  printf("  po wszystkich bledach - zwykla odpowiedz:\n");
  dotknijWykresu();
  od = pokazWykres(nullptr, 1000);
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorca(dzien, 1), "ZEWN"), "wykres, 96 punktow - zaden blad nie zostawil sladu");

  printf("  brak WiFi (centrala wylaczona):\n");
  dotknijWykresu();
  g_atrapa.eter.clear();
  biegnij(4000);
  sprawdz(WiFi.status() != WL_CONNECTED, "panel wie, ze nie ma polaczenia WiFi");
  const int zapytanPrzed = stanC(0).nPanelHist;
  od = pokazWykres(nullptr, 300);
  sprawdz(!wykresWidoczny() && ekranMa("Brak polaczenia z siecia centrali (WiFi)"), "napis od razu: \"Brak polaczenia z siecia centrali (WiFi)\"");
  sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=dzien sposob=centrala przyczyna=brak_wifi", "", "", od) && stanC(0).nPanelHist == zapytanPrzed,
          "log: przyczyna=brak_wifi; zadne zapytanie nie wyszlo");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal w calym scenariuszu");
}

// --- 31 --------------------------------------------------------------------
// Wyjscie z wykresu w trakcie pobierania i pobieranie, ktore nie miesci sie w
// limicie czasu. Starsza centrala i wolne lacze, zeby pobieranie trwalo.
void scenariuszWykresyWyjscieWTrakcie() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  ustawC(0, [](Centrala& c) { c.wersjaHistorii = HIST_302; c.histBajtowNaMs = 15.0; });   // 15 kB/s: 352 kB szloby ok. 23 s
  setup();
  biegnij(3000);
  otworzWykresy();
  kliknij("ZEWN");

  printf("  dotkniecie ekranu w trakcie pobierania:\n");
  size_t od = pokazWykres("TYGODNIOWY", 2000);
  const lv_obj_t* napis = znajdzEtykiete(lv_scr_act(), "Ladowanie danych z centrali...\n");
  const std::string postep1 = napis ? lv_label_get_text(napis) : "";
  biegnij(2000);
  napis = znajdzEtykiete(lv_scr_act(), "Ladowanie danych z centrali...\n");
  const std::string postep2 = napis ? lv_label_get_text(napis) : "";
  info("po 2 s: \"%s\", po 4 s: \"%s\"", postep1.substr(postep1.find('\n') + 1).c_str(), postep2.substr(postep2.find('\n') + 1).c_str());
  sprawdz(!postep1.empty() && !postep2.empty() && postep1 != postep2, "napis \"Ladowanie...\" pokazuje rosnaca liczbe wierszy i kB");
  sprawdz(dotknijWykresu() && ekranMa("POKAZ WYKRES"), "dotkniecie dziala od razu: powrot do wyboru serii");
  biegnij(200);
  Centrala c = stanC(0);
  sprawdz(logMa("tag=WYKRESY zdarzenie=przerwano powod=wyjscie_z_ekranu tryb=tydz", "", "", od), "log: zdarzenie=przerwano powod=wyjscie_z_ekranu");
  // nHistWyslanych == 1 to odpowiedz 302 na /api/history/panel; CSV nie zostal wyslany do konca.
  sprawdz(c.nHistDluga == 1 && c.nHistPorzuconych == 1 && c.nHistWyslanych == 1, "panel zamknal polaczenie z /api/history/long - nie pobiera dalej w tle");

  printf("  zaraz potem inny zakres:\n");
  ustawC(0, [](Centrala& x) { x.histBajtowNaMs = 0; });
  od = pokazWykres("GODZINNY", 3000);
  sprawdz(wykresWidoczny() && tesamePunkty(punktyWykresu(0), zWzorcaV122("godz_zewn"), "ZEWN"), "wykres godzinny poprawny - nic z przerwanego pobierania do niego nie trafilo");

  printf("  pobieranie dluzsze niz limit 20 s (wolne lacze 15 kB/s, 352 kB):\n");
  ustawC(0, [](Centrala& x) { x.histBajtowNaMs = 15.0; });
  dotknijWykresu();
  const int statusPrzed = stanC(0).nStatus;
  od = pokazWykres("TYGODNIOWY", 19000);
  sprawdz(!wykresWidoczny() && znajdzEtykiete(lv_scr_act(), "Ladowanie danych z centrali...\n") != nullptr, "po 19 s jeszcze trwa");
  biegnij(2000);
  sprawdz(wykresWidoczny(), "po 21 s jest wykres z tego, co doszlo");
  bool dopisek = false;
  for (const std::string& s : napisyEkranu()) if (s.find("   NIEPELNE") != std::string::npos) { dopisek = true; info("podpis: \"%s\"", s.c_str()); }
  sprawdz(dopisek, "podpis pod wykresem konczy sie slowem NIEPELNE (v1.2.2 rysowal kawalek bez slowa)");
  sprawdz(logMa("tag=WYKRESY lvl=WARN zdarzenie=pobrano_czesc tryb=tydz sposob=panel powod=limit_czasu etap=c", "", "", od), "log: zdarzenie=pobrano_czesc powod=limit_czasu etap=c");
  info("%s", liniaLogu("tag=WYKRESY lvl=WARN zdarzenie=pobrano_czesc", od).c_str());
  sprawdz(stanC(0).nStatus >= statusPrzed + 8, "przez 21 s pobierania panel dalej odpytywal /api/status");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog (15 s) nie zadzialal, choc pobieranie trwalo 20 s");
  sprawdz(lv_scr_act() != drugiPulpit(), "ekran Wykresy nie zamknal sie sam w trakcie");
  biegnij(31000);
  sprawdz(!wykresWidoczny(), "po 30 s bez dotyku wykres sie zamyka (jak dotad)");
}

// --- 32 --------------------------------------------------------------------
// Alarm w trakcie pobierania danych do wykresu. W v1.2.2 petla stala do konca
// pobierania, wiec panel dowiadywal sie o alarmie dopiero potem.
void scenariuszWykresyAlarmWTrakcie() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  ustawC(0, [](Centrala& c) { c.wersjaHistorii = HIST_302; c.histBajtowNaMs = 15.0; });
  setup();
  biegnij(3000);
  otworzWykresy();
  kliknij("ZEWN");
  const size_t od = pokazWykres("TYGODNIOWY", 2000);
  sprawdz(znajdzEtykiete(lv_scr_act(), "Ladowanie danych z centrali...\n") != nullptr && pamiec::zyweBuforyWykresow() == 8, "pobieranie trwa (8 buforow punktow)");
  ustaw(0, [](Centrala& c) { c.tOgrz = 134.0; c.alarmOgrzewanie = true; });
  biegnij(3000);
  sprawdz(ekranMa("PRZEGRZANIE"), "ekran alarmu pojawia sie w ciagu 3 s, mimo trwajacego pobierania");
  sprawdz(pamiec::zyweBuforyWykresow() == 0, "bufory wykresow zwolnione");
  sprawdz(logMa("tag=WYKRESY zdarzenie=przerwano powod=wyjscie_z_ekranu", "", "", od), "log: zdarzenie=przerwano powod=wyjscie_z_ekranu");
  biegnij(500);
  sprawdz(stanC(0).nHistPorzuconych == 1, "polaczenie z historia zamkniete");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal");
}

// --- 33 --------------------------------------------------------------------
// Stary CSV podany ekranowi Wykresy WPROST, bez sieci - ta sama droga, ktora
// main.cpp podaje linie odebrane od centrali. Dziala na zrodlach od v1.1.0.
//   - na biezacych zrodlach: sprawdza, ze wynik jest taki jak w v1.2.2;
//   - na zrodlach v1.2.2 w trybie obserwacji: ZAPISUJE punkty do pliku
//     punkty.txt w katalogu wynikow - tak powstal wzorce_v122/punkty.txt
//     (polecenie: wzorce_v122/CZYTAJ.txt).
std::string g_csvDoPodania;
template <typename T> struct ArgumentCb;
template <typename A> struct ArgumentCb<void (*)(A)> { typedef A typ; };
void podajCsvWprost(ArgumentCb<WykresyFetchCb>::typ arg) {
#ifdef LVGL_WYKRESY_PUNKTOW
  const bool dlugi = (arg == LVGL_WYKRESY_TRYB_TYDZIEN);
#else
  const bool dlugi = arg;
#endif
  lvgl_dashboard_poc_wykresy_strumien_start(dlugi);
  uint32_t linii = 0;
  for (const std::string& linia : podzielTekst(g_csvDoPodania, '\n')) {
    if (linia.empty()) continue;
    lvgl_dashboard_poc_wykresy_strumien_linia(linia.c_str());
    linii++;
  }
#ifdef LVGL_WYKRESY_PUNKTOW
  lvgl_dashboard_poc_wykresy_strumien_koniec(dlugi, linii, LVGL_WYKRESY_OK, 0);
#else
  lvgl_dashboard_poc_wykresy_strumien_koniec(dlugi, linii);
#endif
}

void scenariuszWykresyStaryCsv() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  setup();
  biegnij(3000);
  { LiczFirmware licz; lvgl_dashboard_poc_ustaw_callback_wykresy_fetch(podajCsvWprost); }
  sprawdz(otworzWykresy(), "ekran Wykresy otwarty");
  kliknij("ZEWN");
  // nieCsv: to nie jest historia. v1.2.2 rysowal z tego wykres (co narysowal,
  // zapisuje wzorzec); biezace zrodla maja pokazac napis.
  struct Przypadek { const char* nazwa; const char* zakres; std::string csv; bool nieCsv; };
  const Przypadek przypadki[] = {
      {"dzien_zewn", "DZIENNY", csvKrotki(1440, -1, 0), false},
      {"godz_zewn", "GODZINNY", csvKrotki(1440, -1, 0), false},
      {"tydz_zewn", "TYGODNIOWY", csvDlugi(2000), false},
      {"dzien_przerwa_zewn", "DZIENNY", csvKrotki(1440, 700, 30), false},
      {"dzien_html_zewn", "DZIENNY", stronaHtml(), true},
  };
  std::string zapis;
  bool pierwszy = true;
  for (const Przypadek& p : przypadki) {
    if (!pierwszy) dotknijWykresu();
    pierwszy = false;
    g_csvDoPodania = p.csv;
    kliknij(p.zakres);
    kliknij("POKAZ WYKRES");
    biegnij(200);
    const std::vector<int> punkty = punktyWykresu(0);
    zapis += std::string(p.nazwa) + ": " + punktyTekst(punkty) + "\n";
    info("%s: %d punktow na %zu pozycjach", p.nazwa, ilePunktow(punkty), punkty.size());
    if (g_obserwacje) continue;
    if (!p.nieCsv) {
      sprawdz(tesamePunkty(punkty, zWzorcaV122(p.nazwa), p.nazwa), (std::string(p.nazwa) + ": punkty takie same jak z v1.2.2").c_str());
      continue;
    }
    // Strona HTML zamiast CSV. Co z niej narysowal v1.2.2, mowi wzorzec
    // (wartosci x10: kazda linia tekstu strony stala sie punktem).
    const std::vector<int> stare = zWzorcaV122(p.nazwa);
    info("%s: v1.2.2 narysowal z tej strony %d punktow: %s", p.nazwa, ilePunktow(stare), punktyTekst(std::vector<int>(stare.begin(), stare.begin() + std::min<size_t>(stare.size(), 10))).c_str());
    sprawdz(ilePunktow(stare) > 0, (std::string(p.nazwa) + ": (wzorzec) v1.2.2 rysowal ze strony HTML wykres").c_str());
    sprawdz(!wykresWidoczny() && ekranMa("Centrala odpowiedziala czyms innym niz dane wykresu"),
            (std::string(p.nazwa) + ": teraz napis \"Centrala odpowiedziala czyms innym niz dane wykresu\", wykresu nie ma").c_str());
  }
  if (!g_katalogWyjscia.empty()) {
    const std::string sciezka = g_katalogWyjscia + "/punkty.txt";
    if (FILE* f = fopen(sciezka.c_str(), "wb")) { fwrite(zapis.data(), 1, zapis.size(), f); fclose(f); info("punkty zapisane: %s", sciezka.c_str()); }
  }
}

// --- 34 --------------------------------------------------------------------
// Centrala 3.58.5 tuz po restarcie, z wylaczonym zapisem historii na flash i
// starymi plikami sprzed 3 dni. Od 3.58.5 centrala konczy okno wykresu na
// BIEZACYM czasie, wiec panel dostaje odpowiedzi, jakich 3.58.4 nie wysylala:
// same linie "#przerwa" (ostatnia godzina) i przerwe siegajaca konca osi
// (tydzien). Firmware panelu jest ten sam (v1.3.0) - scenariusz sprawdza, ze
// radzi sobie z nimi bez zmian.
void scenariuszWykresyPoRestarcieCentrali() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  const std::string godz = wzorzec("godz_po_restarcie_pusto.txt"), tydz = wzorzec("tydz_po_restarcie.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["godz"] = godz; c.panelHist["tydz"] = tydz; });
  setup();
  biegnij(3000);
  otworzWykresy();
  kliknij("ZEWN");

  printf("  ostatnia godzina: centrala ma tylko historie sprzed 3 dni - odpowiedz to 96 linii \"#przerwa\":\n");
  const Centrala przed = stanC(0);
  size_t od = pokazWykres("GODZINNY", 1000);
  sprawdz(ilePrzerwWeWzorcu(godz) == 96 && ilePunktow(zWzorca(godz, 1)) == 0, "odpowiedz centrali: 96 przerw, zadnego wiersza z danymi");
  sprawdz(!wykresWidoczny() && ekranMa("Centrala nie ma jeszcze danych z tego zakresu\n(pierwszy pomiar zapisuje minute po starcie)\n\n"
                                       "Zapis historii na flash jest w centrali wylaczony:\npo jej restarcie historia zaczyna sie od zera"),
          "zamiast wykresu napis: \"Centrala nie ma jeszcze danych z tego zakresu...\" z dopowiedzeniem o wylaczonym zapisie (fs=0)");
  sprawdz(logMa("tag=WYKRESY zdarzenie=brak_danych tryb=godz sposob=centrala przyczyna=pusto", "", "", od) && !logMa("zdarzenie=blad", "", "", od),
          "log: zdarzenie=brak_danych przyczyna=pusto (to nie blad)");
  sprawdz(stanC(0).nHistKrotka == przed.nHistKrotka && stanC(0).nHistDluga == przed.nHistDluga, "panel nie pobiera wtedy calego CSV po staremu");
  zrzutEkranu("po_restarcie_godzina");

  printf("  tydzien: 4 dni starej historii, potem 3 dni bez danych az do teraz:\n");
  dotknijWykresu();
  od = pokazWykres("TYGODNIOWY", 1000);
  const std::vector<int> p = punktyWykresu(0);
  sprawdz(ilePrzerwWeWzorcu(tydz) == 41 && wykresWidoczny() && tesamePunkty(p, zWzorca(tydz, 1), "ZEWN"),
          "96 pozycji: 55 punktow i 41 przerw, dokladnie w miejscach podanych przez centrale");
  sprawdz(p.size() == 96 && p[0] == 20 && p[54] == 40 && p[55] == BRAK && p[95] == BRAK,
          "linia konczy sie po 4 dniach (55. pozycja z 96); ostatnie 3 dni sa puste - nie dociagniete do prawej krawedzi");
  sprawdz(ekranMa("06.11 13:00  -  13.11 13:00"), "podpis: \"06.11 13:00  -  13.11 13:00\" - os konczy sie TERAZ, nie na ostatnim starym pomiarze");
  sprawdz(logMa("tag=WYKRESY zdarzenie=pobrano tryb=tydz sposob=centrala punktow=55 przerw=41", "", "", od), "log: punktow=55 przerw=41");
  sprawdz(g_atrapa.wdtZadzialal == 0, "watchdog nie zadzialal");
  zrzutEkranu("po_restarcie_tydzien");
}

#ifdef POMIAR_STERTY
// --- pomiar pamieci (tylko pomiar_pamieci.sh) -------------------------------
// 32-bitowy model firmware'u z licznikiem sterty. "Na ESP32" = tyle zajmuja te
// same bloki na sterycie ESP-IDF 5 (rozmiar do 4 bajtow w gore + 4 bajty
// naglowka). Nie ma tu WiFi ani lwIP plytki - one biora swoje osobno.
uint32_t g_pomiarBazaEsp = 0, g_pomiarBazaBloki = 0;
// Linia "POMIAR nazwa zywe=... blokow=... szczyt=..." - czyta ja pomiar_pamieci.sh.
// zywe   = ile pamieci firmware trzyma w tej chwili,
// szczyt = najwiecej naraz od poprzedniej migawki.
void migawka(const char* nazwa) {
  const uint32_t esp = pomiar_zywe_esp() - g_pomiarBazaEsp;
  const uint32_t szczyt = pomiar_szczyt_esp() - g_pomiarBazaEsp;
  printf("POMIAR %s zywe=%u blokow=%u szczyt=%u\n", nazwa, esp, pomiar_zywe_bloki() - g_pomiarBazaBloki, szczyt);
  if (!g_katalogWyjscia.empty()) {
    pomiar_zrzut((g_katalogWyjscia + "/pamiec_" + nazwa + ".txt").c_str(), 0);
    pomiar_zrzut((g_katalogWyjscia + "/pamiec_" + nazwa + "_szczyt.txt").c_str(), 1);
  }
  pomiar_zeruj_szczyty();
}

void scenariuszPomiarPamieci() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -33, 0));
  g_pomiarBazaEsp = pomiar_zywe_esp();
  g_pomiarBazaBloki = pomiar_zywe_bloki();
  pomiar_zeruj_szczyty();
  {
    LiczFirmware licz;
    setup();
  }
  migawka("po_setup");
  biegnij(20000);
  migawka("po_20s");
  biegnij(10000);
  migawka("cykl_10s");
  biegnij(600000);
  migawka("po_10min");
  ustaw(0, [](Centrala& c) { c.tOgrz = 134.0; c.alarmOgrzewanie = true; c.symulacja = SYM_OGRZ; });
  biegnij(10000);
  migawka("alarm_przegrzania");
  ustaw(0, [](Centrala& c) { c.tOgrz = 55.0; c.alarmOgrzewanie = false; c.symulacja = 0; });
  biegnij(10000);
  migawka("po_alarmie");
  // Ekran Wykresy: bufory punktow istnieja tylko, gdy jest otwarty.
  lv_obj_t* drugi = nullptr;
  if (lv_disp_t* d = lv_disp_get_default()) {
    for (uint32_t i = 0; i < d->screen_cnt; i++)
      if (znajdzEtykiete(d->screens[i], "WYKRESY") != nullptr && znajdzEtykiete(d->screens[i], "WYSWIETLACZ") != nullptr) drugi = d->screens[i];
  }
  if (drugi != nullptr) {
    { LiczFirmware licz; lv_scr_load(drugi); }
    kliknij("WYKRESY");
    biegnij(2000);
    migawka("wykresy_otwarte");
    biegnij(30000);   // bezczynnosc zamyka Wykresy, potem drugi pulpit
    migawka("wykresy_zamkniete");
    // Jeszcze dwa razy: wielokrotne otwieranie nie moze zostawiac pamieci.
    for (int i = 0; i < 2; i++) {
      { LiczFirmware licz; lv_scr_load(drugi); }
      kliknij("WYKRESY");
      biegnij(32000);
    }
    migawka("wykresy_3_razy");

    // Pobranie danych do wykresu: ile pamieci bierze samo pobieranie i
    // rysowanie. Najpierw gotowe punkty z centrali 3.58.4, potem to samo po
    // staremu (caly CSV, 352 kB) ze starszej centrali.
    const std::string dzien = wzorzec("dzien.txt");
    ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = dzien; });
    { LiczFirmware licz; lv_scr_load(drugi); }
    kliknij("WYKRESY");
    biegnij(300);
    kliknij("ZEWN");
    kliknij("BOJLER");
    migawka("wykres_przed");
    kliknij("POKAZ WYKRES");
    biegnij(1000);
    printf("POMIAR-INFO wykres_nowy widoczny=%d punktow=%d\n", wykresWidoczny() ? 1 : 0, ilePunktow(punktyWykresu(0)));
    migawka("wykres_nowy");
    dotknijWykresu();
    ustawC(0, [](Centrala& c) { c.wersjaHistorii = HIST_302; });
    kliknij("TYGODNIOWY");
    migawka("wykres_stary_przed");
    kliknij("POKAZ WYKRES");
    biegnij(3000);
    printf("POMIAR-INFO wykres_stary widoczny=%d punktow=%d\n", wykresWidoczny() ? 1 : 0, ilePunktow(punktyWykresu(0)));
    migawka("wykres_stary");
    biegnij(60000);   // bezczynnosc zamyka wykres, ekran Wykresy i drugi pulpit
    migawka("wykres_po_zamknieciu");
    // Jeszcze piec wykresow (na zmiane oba sposoby): kolejne nie moga zostawiac pamieci.
    for (int i = 0; i < 5; i++) {
      ustawC(0, [i](Centrala& c) { c.wersjaHistorii = (i % 2 == 0) ? HIST_PANEL : HIST_302; });
      { LiczFirmware licz; lv_scr_load(drugi); }
      kliknij("WYKRESY");
      biegnij(300);
      kliknij(i % 2 == 0 ? "DZIENNY" : "TYGODNIOWY");
      kliknij("POKAZ WYKRES");
      biegnij(3000);
      printf("POMIAR-INFO wykres_powtorka_%d widoczny=%d punktow=%d\n", i, wykresWidoczny() ? 1 : 0, ilePunktow(punktyWykresu(0)));
      biegnij(60000);
    }
    migawka("wykres_powtorki");
  }
}

// --- czy RAM-u starczy: przypadki (pomiar_pamieci.sh --przypadki) -----------
// Kazdy przypadek zostawia linie
//   PRZYPADEK nazwa szczyt=B po=B ...
// szczyt = najwiecej pamieci naraz w trakcie przypadku PONAD stan spoczynku
//          (pulpit glowny po starcie, miedzy odpytaniami centrali),
// po     = ile zostaje po przypadku ponad stan spoczynku.
// Liczone sa bloki, o ktore prosi firmware, LVGL i ArduinoJson ("bajty ESP",
// patrz pomiar_sterty.c). Pamieci WiFi i lwIP plytki tu nie ma - doliczana
// jest osobno w pomiar_pamieci.sh (SIEC_*).
// Calosc biegnie dwa razy: druga runda (linie PRZYPADEK2) pokazuje, czy po
// powtorzeniu tych samych czynnosci cos przybywa.
uint32_t g_przypBaza = 0;
const char* g_przypZnacznik = "PRZYPADEK";
void przypadekStart() { pomiar_zeruj_szczyty(); }
void przypadek(const char* nazwa, const char* fmt = nullptr, ...) __attribute__((format(printf, 2, 3)));
void przypadek(const char* nazwa, const char* fmt, ...) {
  char uwagi[500] = "";
  if (fmt != nullptr) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(uwagi, sizeof(uwagi), fmt, ap);
    va_end(ap);
  }
  printf("%s %s szczyt=%d po=%d %s\n", g_przypZnacznik, nazwa, (int)(pomiar_szczyt_esp() - g_przypBaza), (int)(pomiar_zywe_esp() - g_przypBaza), uwagi);
  fflush(stdout);
  pomiar_zeruj_szczyty();
}
// Wraca na pulpit glowny tak, jak robi to sam panel: przez bezczynnosc.
void wrocNaPulpit(lv_obj_t* pulpit) {
  for (int i = 0; i < 4 && lv_scr_act() != pulpit; i++) biegnij(16000);
  if (lv_scr_act() != pulpit) { LiczFirmware licz; lv_scr_load(pulpit); }
  biegnij(500);
}
// Ekran Wykresy otwarty na wyborze serii, z wybranymi 8 seriami (najwiecej,
// ile pozwala panel: 7 temperatur i cisnienie). Wybor serii zostaje w panelu
// po zamknieciu ekranu, wiec klikany jest tylko raz.
const char* const OSIEM_SERII[8] = {"ZEWN", "BOJLER", "Temp. C.O", "Temp. C.O Sr.", "POWROT", "PANEL", "Wewnetrzna", "CISNIENIE"};
bool g_serieWybrane = false;
bool wykresyGotowe() {
  if (znajdzEtykiete(lv_scr_act(), "POKAZ WYKRES") == nullptr) {
    dotknijWykresu();
    if (znajdzEtykiete(lv_scr_act(), "POKAZ WYKRES") == nullptr && !otworzWykresy()) return false;
  }
  if (!g_serieWybrane) {
    for (const char* n : OSIEM_SERII) kliknij(n);
    g_serieWybrane = true;
    biegnij(300);
  }
  return true;
}
int ileSeriiZPunktami() {
  int n = 0;
  for (int k = 0; k < 8; k++) if (ilePunktow(punktyWykresu(k)) > 0) n++;
  return n;
}
// Linia logu z wynikiem pobierania (nie "zdarzenie=start").
std::string wynikWykresuZLogu(size_t od) {
  for (const char* z : {"zdarzenie=pobrano ", "zdarzenie=pobrano_czesc", "zdarzenie=brak_danych", "zdarzenie=blad", "zdarzenie=przerwano"}) {
    for (const std::string& linia : podzielTekst(g_atrapa.serial.substr(od), '\n'))
      if (linia.find("tag=WYKRESY") != std::string::npos && linia.find(z) != std::string::npos) {
        std::string w = linia.substr(linia.find("zdarzenie="));
        const size_t ram = w.find(" ram=");          // "ram=" w tym modelu to stala atrapy, nie pomiar
        if (ram != std::string::npos) w.resize(ram);
        const size_t msg = w.find(" msg=");
        if (msg != std::string::npos) w.resize(msg);
        return w;
      }
  }
  return "(brak)";
}
// Jeden wykres: klikniecie zakresu i "POKAZ WYKRES", czekanie, pomiar.
void przypadekWykres(const char* nazwa, const char* zakres, unsigned long czekajMs) {
  if (!wykresyGotowe()) { printf("PRZYPADEK-BLAD %s: ekran Wykresy sie nie otworzyl\n", nazwa); return; }
  const int statusPrzed = stanC(0).nStatus;
  przypadekStart();
  const size_t od = pokazWykres(zakres, czekajMs);
  std::string napis;
  if (!wykresWidoczny()) for (const std::string& x : napisyEkranu()) if (x.size() > 12 && x.find("POKAZ") == std::string::npos && x[0] != '[') { napis = x; break; }
  if (napis.find('\n') != std::string::npos) napis.resize(napis.find('\n'));
  przypadek(nazwa, "wykres=%d serii=%d punktow=%d statusow_w_trakcie=%d log=\"%s\" napis=\"%s\"", wykresWidoczny() ? 1 : 0, ileSeriiZPunktami(),
            ilePunktow(punktyWykresu(0)), stanC(0).nStatus - statusPrzed, wynikWykresuZLogu(od).c_str(), napis.c_str());
}

void rundaPrzypadkow(lv_obj_t* pulpit) {
  const std::string dzien = wzorzec("dzien.txt"), godz = wzorzec("godz_ta_sama_doba.txt"), tydz = wzorzec("tydz_pelne_bufory.txt");
  ustawC(0, [&](Centrala& c) { c.wersjaHistorii = HIST_PANEL; c.panelHist["dzien"] = dzien; c.panelHist["godz"] = godz; c.panelHist["tydz"] = tydz; });

  // --- praca bez dotyku ------------------------------------------------------
  { const int s0 = stanC(0).nStatus, u0 = stanC(0).nUstawienia;
    przypadekStart();
    biegnij(600000);
    przypadek("praca_10_min", "statusow=%d ustawien=%d", stanC(0).nStatus - s0, stanC(0).nUstawienia - u0); }

  przypadekStart();
  ustaw(0, [](Centrala& c) { c.tOgrz = 134.0; c.alarmOgrzewanie = true; c.symulacja = SYM_OGRZ; });
  biegnij(10000);
  przypadek("alarm_przegrzania", "ekran_alarmu=%d", ekranMa("PRZEGRZANIE") ? 1 : 0);
  ustaw(0, [](Centrala& c) { c.tOgrz = 55.0; c.alarmOgrzewanie = false; c.symulacja = 0; });
  biegnij(10000);
  wrocNaPulpit(pulpit);

  // --- kazdy ekran po kolei --------------------------------------------------
  // Wszystkie ekrany sa zbudowane przy starcie; tu widac, ile pamieci LVGL
  // bierze chwilowo na samo ich narysowanie.
  if (lv_disp_t* d = lv_disp_get_default()) {
    for (uint32_t i = 0; i < d->screen_cnt; i++) {
      lv_obj_t* e = d->screens[i];
      if (e == pulpit || lv_obj_get_child_cnt(e) == 0 || znajdzEtykiete(e, "POKAZ WYKRES") != nullptr) continue;   // Wykresy osobno, nizej
      std::string tytul;
      { std::vector<std::string> w; zbierzNapisy(e, w); for (const std::string& x : w) if (!x.empty()) { tytul = x; break; } }
      { std::string czysty; for (char c : tytul) { if (isalnum((unsigned char)c)) czysty += c; else if (c == ' ' || c == '\n') czysty += '_'; } tytul = czysty; }
      if (tytul.empty() || znajdzEtykiete(e, "WYSWIETLACZ") != nullptr && znajdzEtykiete(e, "WYKRESY") != nullptr) tytul = "drugi_pulpit";
      przypadekStart();
      { LiczFirmware licz; lv_scr_load(e); }
      biegnij(3000);
      przypadek(("ekran_" + tytul).c_str());
      { LiczFirmware licz; lv_scr_load(pulpit); }
      biegnij(1000);
    }
  }
  // Polecenie z dotyku (POST do centrali): kafelek POMPA -> WLACZ.
  przypadekStart();
  { const int postPrzed = stanC(0).nPost;
    const bool k1 = kliknij("POMPA"); biegnij(500);
    const bool k2 = kliknij("WLACZ"); biegnij(3000);
    przypadek("polecenie_z_dotyku", "klik=%d%d postow=%d", k1 ? 1 : 0, k2 ? 1 : 0, stanC(0).nPost - postPrzed); }
  wrocNaPulpit(pulpit);

  // --- ekran Wykresy ---------------------------------------------------------
  przypadekStart();
  const bool otwarty = otworzWykresy();
  biegnij(2000);
  przypadek("wykresy_otwarte", "otwarty=%d bufory=%d", otwarty ? 1 : 0, pamiec::zyweBuforyWykresow());

  // Gotowe punkty z centrali 3.58.5, szybkie lacze, 8 serii.
  przypadekWykres("wykres_dzien", "DZIENNY", 3000);
  przypadekStart(); biegnij(10000); przypadek("wykres_na_ekranie_10_s", "wykres=%d", wykresWidoczny() ? 1 : 0);
  przypadekWykres("wykres_godzina", "GODZINNY", 3000);
  przypadekWykres("wykres_tydzien_pelne_bufory", "TYGODNIOWY", 3000);
  const std::string przerwa = wzorzec("tydz_przerwa.txt"), pusto = wzorzec("godz_po_restarcie_pusto.txt"), brak = wzorzec("dzien_pusta.txt"), jedna = wzorzec("godz_jedna_probka.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["tydz"] = przerwa; c.panelHist["godz"] = pusto; c.panelHist["dzien"] = brak; });
  przypadekWykres("wykres_tydzien_z_przerwa", "TYGODNIOWY", 3000);
  przypadekWykres("napis_same_przerwy", "GODZINNY", 3000);
  przypadekWykres("napis_centrala_bez_historii", "DZIENNY", 3000);
  ustawC(0, [&](Centrala& c) { c.panelHist["godz"] = jedna; });
  przypadekWykres("napis_jeden_pomiar", "GODZINNY", 3000);
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = dzien; c.panelHist["godz"] = godz; c.panelHist["tydz"] = tydz; });

  // Bledy po stronie centrali i sieci.
  ustawC(0, [](Centrala& c) { c.panelHistKod = 503; });
  przypadekWykres("blad_centrala_503", "DZIENNY", 3000);
  ustawC(0, [](Centrala& c) { c.panelHistKod = 200; c.histHtml = true; });
  przypadekWykres("blad_strona_html", "DZIENNY", 3000);
  ustawC(0, [](Centrala& c) { c.histHtml = false; c.histUrwijPo = 6000; });
  przypadekWykres("blad_odpowiedz_urwana", "DZIENNY", 3000);
  ustawC(0, [](Centrala& c) { c.histUrwijPo = -1; c.histZamilknijPo = 6000; });
  przypadekWykres("blad_centrala_zamilkla", "DZIENNY", 9000);
  ustawC(0, [](Centrala& c) { c.histZamilknijPo = -1; });

  // Wolne lacze: wykres idzie kilka-kilkanascie sekund, w tym czasie panel
  // dalej odpytuje /api/status i /api/ustawienia-get.
  ustawC(0, [](Centrala& c) { c.histBajtowNaMs = 2.0; });
  przypadekWykres("wykres_wolne_lacze_2_kB_s", "DZIENNY", 12000);
  ustawC(0, [](Centrala& c) { c.histBajtowNaMs = 0.8; });
  przypadekWykres("wykres_wolne_lacze_limit_20_s", "TYGODNIOWY", 22000);

  // Najgorszy zbieg: koniec pobierania i rysowanie wykresu w roznych chwilach
  // wzgledem odpytan centrali. Szukany jest najwiekszy szczyt.
  uint32_t najwZbieg = 0; int najwOdstep = 0; double najwTempo = 0; int prob = 0, bezWykresu = 0;
  const double TEMPA[] = {1.5, 3.0, 6.0, 12.0, 0.0};
  for (double tempo : TEMPA) {
    ustawC(0, [tempo](Centrala& c) { c.histBajtowNaMs = tempo; });
    for (int odstep = 0; odstep < 2000; odstep += 125) {
      if (!wykresyGotowe()) continue;
      biegnij(200 + (unsigned long)odstep);
      przypadekStart();
      pokazWykres("DZIENNY", tempo > 0 ? (unsigned long)(19000.0 / tempo) + 1500 : 1500);
      const uint32_t szczyt = pomiar_szczyt_esp() - g_przypBaza;
      prob++;
      if (!wykresWidoczny()) bezWykresu++;
      if (szczyt > najwZbieg) { najwZbieg = szczyt; najwOdstep = odstep; najwTempo = tempo; }
    }
  }
  ustawC(0, [](Centrala& c) { c.histBajtowNaMs = 0; });
  pomiar_zeruj_szczyty();
  printf("%s wykres_najgorszy_zbieg szczyt=%u po=%d prob=%d bez_wykresu=%d tempo_kB_s=%.1f odstep_ms=%d\n", g_przypZnacznik, najwZbieg,
         (int)(pomiar_zywe_esp() - g_przypBaza), prob, bezWykresu, najwTempo, najwOdstep);

  // Alarm w trakcie pobierania: ekran alarmu zastepuje Wykresy.
  ustawC(0, [](Centrala& c) { c.histBajtowNaMs = 1.0; });
  if (wykresyGotowe()) {
    przypadekStart();
    pokazWykres("DZIENNY", 3000);
    ustaw(0, [](Centrala& c) { c.tOgrz = 134.0; c.alarmOgrzewanie = true; c.symulacja = SYM_OGRZ; });
    biegnij(6000);
    przypadek("alarm_w_trakcie_pobierania", "ekran_alarmu=%d bufory=%d", ekranMa("PRZEGRZANIE") ? 1 : 0, pamiec::zyweBuforyWykresow());
    ustaw(0, [](Centrala& c) { c.tOgrz = 55.0; c.alarmOgrzewanie = false; c.symulacja = 0; });
    biegnij(10000);
  }
  ustawC(0, [](Centrala& c) { c.histBajtowNaMs = 0; });
  wrocNaPulpit(pulpit);

  // Centrala starsza niz 3.58.4: caly CSV po staremu. 8000 wierszy to pelny
  // bufor dlugi centrali, 1440 - pelny bufor krotki.
  ustawC(0, [](Centrala& c) { c.wersjaHistorii = HIST_302; c.wierszyDlugich = 8000; c.wierszyKrotkich = 1440; });
  przypadekWykres("stara_centrala_tydzien_caly_csv", "TYGODNIOWY", 8000);
  przypadekWykres("stara_centrala_doba_caly_csv", "DZIENNY", 4000);
  ustawC(0, [](Centrala& c) { c.histBajtowNaMs = 40.0; });   // 40 kB/s: caly CSV nie zdazy w 20 s
  przypadekWykres("stara_centrala_tydzien_limit_20_s", "TYGODNIOWY", 22000);
  ustawC(0, [](Centrala& c) { c.histBajtowNaMs = 0; c.wersjaHistorii = HIST_PANEL; });

  // 30 wykresow z rzedu, na zmiane trzy zakresy: nic nie moze przybywac.
  const char* const ZAKRESY[3] = {"GODZINNY", "DZIENNY", "TYGODNIOWY"};
  if (wykresyGotowe()) {
    pokazWykres("DZIENNY", 1500);
    const uint32_t przed = pomiar_zywe_esp();
    przypadekStart();
    int narysowanych = 0;
    for (int i = 0; i < 30; i++) {
      if (!wykresyGotowe()) break;
      pokazWykres(ZAKRESY[i % 3], 1500);
      if (wykresWidoczny()) narysowanych++;
    }
    przypadek("wykresy_30_z_rzedu", "narysowanych=%d przyrost=%d", narysowanych, (int)(pomiar_zywe_esp() - przed));
  }
  wrocNaPulpit(pulpit);
  biegnij(5000);
  przypadek("po_wszystkim_na_pulpicie", "bufory=%d restart=%d wdt=%d", pamiec::zyweBuforyWykresow(), g_poRestarcie ? 1 : 0, g_atrapa.wdtZadzialal);
}

// Stan spoczynku = najmniej zywej pamieci w ciagu 6 s (miedzy odpytaniami centrali).
uint32_t zyweWSpoczynku() {
  uint32_t najmniej = pomiar_zywe_esp();
  for (int i = 0; i < 600; i++) { biegnij(10); if (pomiar_zywe_esp() < najmniej) najmniej = pomiar_zywe_esp(); }
  return najmniej;
}

void scenariuszPomiarPrzypadki() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -33, 0));
  const uint32_t przedSetup = pomiar_zywe_esp();
  { LiczFirmware licz; setup(); }
  biegnij(20000);
  lv_obj_t* pulpit = lv_scr_act();
  g_przypBaza = zyweWSpoczynku();
  printf("PRZYPADEK-BAZA po_starcie=%u blokow=%u\n", g_przypBaza - przedSetup, pomiar_zywe_bloki());
  printf("PRZYPADEK-INFO stary_csv_dlugi_bajtow=%zu stary_csv_krotki_bajtow=%zu\n", csvDlugi(8000).size(), csvKrotki(1440, -1, 0).size());
  rundaPrzypadkow(pulpit);
  const uint32_t poPierwszej = zyweWSpoczynku();
  g_przypZnacznik = "PRZYPADEK2";
  rundaPrzypadkow(pulpit);
  const uint32_t poDrugiej = zyweWSpoczynku();
  printf("PRZYPADEK-PRZYROST po_pierwszej_rundzie=%d druga_runda=%d\n", (int)(poPierwszej - g_przypBaza), (int)(poDrugiej - poPierwszej));

  // Dluga praca: doba bez dotyku, potem 300 wykresow z rzedu.
  g_przypZnacznik = "PRZYPADEK";
  { const int s0 = stanC(0).nStatus;
    przypadekStart();
    biegnij(24UL * 3600UL * 1000UL);
    przypadek("praca_24_h", "statusow=%d przyrost=%d", stanC(0).nStatus - s0, (int)(zyweWSpoczynku() - poDrugiej)); }
  const char* const ZAKRESY[3] = {"GODZINNY", "DZIENNY", "TYGODNIOWY"};
  if (wykresyGotowe()) {
    pokazWykres("DZIENNY", 1500);
    const uint32_t przed = pomiar_zywe_esp();
    przypadekStart();
    int narysowanych = 0;
    for (int i = 0; i < 300; i++) {
      if (!wykresyGotowe()) break;
      pokazWykres(ZAKRESY[i % 3], 1200);
      if (wykresWidoczny()) narysowanych++;
    }
    przypadek("wykresy_300_z_rzedu", "narysowanych=%d przyrost=%d", narysowanych, (int)(pomiar_zywe_esp() - przed));
  }
  wrocNaPulpit(pulpit);
  przypadek("koniec_na_pulpicie", "przyrost_od_drugiej_rundy=%d restart=%d wdt=%d", (int)(zyweWSpoczynku() - poDrugiej), g_poRestarcie ? 1 : 0, g_atrapa.wdtZadzialal);
}

// --- co sie dzieje, gdy wolnego RAM-u jest mniej (pomiar_pamieci.sh --przypadki)
// Sterta modelu dostaje limit: tyle, ile firmware trzyma w spoczynku, plus
// LCD_WOLNY_RAM_B bajtow. Przydzial ponad limit dostaje odmowe - jak malloc()
// na plytce bez wolnej pamieci. Potem zwykle czynnosci: praca, ekran Wykresy,
// wykres dzienny z 8 seriami, ogladanie go, powrot na pulpit.
// Odmowa nie dotyczy atrapy String (patrz pomiar_sterty.c) - brak pamieci na
// odebranie odpowiedzi sprawdza osobno scenariusz brak_pamieci_odbior.
void scenariuszPomiarMaloRamu() {
  const char* zm = getenv("LCD_WOLNY_RAM_B");
  const uint32_t wolne = (zm != nullptr) ? (uint32_t)strtoul(zm, nullptr, 10) : 20000;
  pomiar_lap_awarie();
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -33, 0));
  const std::string dzien = wzorzec("dzien.txt");
  ustawC(0, [&](Centrala& c) { c.panelHist["dzien"] = dzien; });
  { LiczFirmware licz; setup(); }
  biegnij(20000);
  lv_obj_t* pulpit = lv_scr_act();
  const uint32_t baza = zyweWSpoczynku();
  pomiar_limit(baza + wolne);
  const size_t od = g_atrapa.serial.size();
  auto etap = [&](const char* nazwa, const char* dodatkowo) {
    printf("MALO-RAMU wolne=%u etap=%s odmow=%u restart=%d %s\n", wolne, nazwa, pomiar_odmow(), g_poRestarcie ? 1 : 0, dodatkowo);
    fflush(stdout);
  };
  char b[200];
  // ESP.restart() wywolany z obslugi dotyku (poza loop()) - np. LVGL bez
  // pamieci przy otwieraniu ekranu: scenariusz ma to zapisac i isc dalej.
  auto dotyk = [&](const std::function<void()>& co) {
    if (g_poRestarcie) return;
    try { co(); } catch (const AtrapaRestart&) { g_poRestarcie = true; }
  };
  // 1. zwykla praca
  int s0 = stanC(0).nStatus;
  biegnij(30000);
  snprintf(b, sizeof(b), "statusow=%d brak_pamieci_w_logu=%d", stanC(0).nStatus - s0, ileLinii("przyczyna=pam", "", "", od) + ileLinii("NoMemory", "", "", od));
  etap("praca_30_s", b);
  // 2. ekran Wykresy
  bool otwarty = false;
  dotyk([&] { otwarty = otworzWykresy(); biegnij(2000); });
  snprintf(b, sizeof(b), "otwarty=%d bufory=%d brak_ram_w_logu=%d", otwarty ? 1 : 0, pamiec::zyweBuforyWykresow(), ileLinii("[WYKRESY] BLAD: brak RAM", "", "", od));
  etap("wykresy_otwarte", b);
  // 3. wykres dzienny, 8 serii
  size_t odW = g_atrapa.serial.size();
  if (otwarty) dotyk([&] { for (const char* n : OSIEM_SERII) kliknij(n); biegnij(300); odW = pokazWykres("DZIENNY", 3000); });
  std::string napis;
  if (!g_poRestarcie && !wykresWidoczny()) for (const std::string& x : napisyEkranu()) if (x.size() > 12 && x.find("POKAZ") == std::string::npos && x[0] != '[') { napis = x; break; }
  if (napis.find('\n') != std::string::npos) napis.resize(napis.find('\n'));
  snprintf(b, sizeof(b), "wykres=%d serii=%d log=\"%.60s\" napis=\"%.60s\"", (!g_poRestarcie && wykresWidoczny()) ? 1 : 0, g_poRestarcie ? 0 : ileSeriiZPunktami(),
           wynikWykresuZLogu(odW).c_str(), napis.c_str());
  etap("wykres_dzien", b);
  // 4. ogladanie wykresu 20 s: panel dalej odpytuje centrale
  s0 = stanC(0).nStatus;
  const int pamPrzed = ileLinii("przyczyna=pam", "", "", od) + ileLinii("NoMemory", "", "", od);
  biegnij(20000);
  snprintf(b, sizeof(b), "statusow=%d brak_pamieci_w_logu=%d", stanC(0).nStatus - s0, ileLinii("przyczyna=pam", "", "", od) + ileLinii("NoMemory", "", "", od) - pamPrzed);
  etap("wykres_na_ekranie_20_s", b);
  // 5. powrot na pulpit i dalsza praca
  dotyk([&] { wrocNaPulpit(pulpit); biegnij(20000); });
  snprintf(b, sizeof(b), "na_pulpicie=%d bufory=%d wdt=%d", (!g_poRestarcie && lv_scr_act() == pulpit) ? 1 : 0, pamiec::zyweBuforyWykresow(), g_atrapa.wdtZadzialal);
  etap("po_powrocie", b);
  pomiar_limit(0);
  std::string restart = liniaLogu("zdarzenie=restart", od);
  if (restart == "(brak)" || restart.empty()) restart = liniaLogu("LVGL", od);
  printf("MALO-RAMU wolne=%u koniec odmow=%u restart=%d log_restartu=\"%.160s\"\n", wolne, pomiar_odmow(), g_poRestarcie ? 1 : 0, restart.c_str());
}
#endif

// ===========================================================================
// Aktualizacja OTA panelu przez centrale
// ===========================================================================
// Firmware panelu patrzy na flage "panel_ota_dostepna" RAZ: na koncu setup()
// (sprawdzIWykonajOtaPanelu()). Te scenariusze sprawdzaja, KIEDY panel zaczyna
// aktualizacje, o jaki adres pyta i co robi potem. Atrapa HTTPUpdate niczego
// nie pobiera i nie zapisuje (mock/HTTPUpdate.h) - samego pobierania, zapisu
// do flash i bootloadera tu NIE MA; to sprawdzi dopiero plytka.
// Scenariusze z dopiskiem "STAN OBECNY" opisuja zachowanie, ktore jest dzis,
// a nie takie, jakie byc powinno - po zmianie firmware'u trzeba je poprawic.
const char OTA_TAG_WYDANIA[] = "3.58.6";   // przykladowy tag wydania na GitHubie (dotychczasowe: "3.57.9", bez "v")
const char OTA_ADRES[] = "http://192.168.4.1:80/panel-ota.bin?token=";

void otaCentralaCzekaNa(const char* tag) {
  std::lock_guard<std::mutex> blokada(g_mutex);
  g_centrale[0].panelOtaDostepna = true;
  g_centrale[0].panelOtaWersja = tag;
}

// setup() z aktualizacja konczy sie ESP.restart() - atrapa zglasza to wyjatkiem.
bool otaSetupSkonczylSieRestartem() {
  try {
    setup();
  } catch (const AtrapaRestart&) {
    g_poRestarcie = true;
    return true;
  }
  return false;
}

// Ekran w czasie pobierania: LVGL rysuje dopiero w loop() (lvgl_port_tick()),
// a start z aktualizacja do loop() nie dochodzi.
bool otaEkranPusty() {
  bool jednolity = true;
  for (int i = 1; i < 480 * 320 && jednolity; i++) jednolity = (g_atrapa.ekran[i] == g_atrapa.ekran[0]);
  info("ekran w czasie pobierania: %s (kolor RGB565 0x%04X)", jednolity ? "jednolite tlo, bez napisu i bez kafelkow" : "cos narysowane", (unsigned)g_atrapa.ekran[0]);
  return jednolity;
}

bool otaAdresPoprawny() {
  const size_t n = sizeof(OTA_ADRES) - 1;
  const bool ok = g_atrapa.otaAdres.rfind(OTA_ADRES, 0) == 0 && g_atrapa.otaAdres.size() > n;
  // Hasla OTA nie wypisujemy (z prawdziwym src/secrets.h byloby w logu testu).
  info("adres pobierania: %s<haslo OTA, %zu znakow>", OTA_ADRES, ok ? g_atrapa.otaAdres.size() - n : (size_t)0);
  return ok;
}

// Centrala ma juz na dysku firmware panelu (po /update_panel w Telegramie) i
// zglasza to w /api/status. Panel zostaje wlaczony albo zrestartowany.
void scenariuszOtaStart() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  otaCentralaCzekaNa(OTA_TAG_WYDANIA);
  g_atrapa.otaWynik = 2;        // HTTP_UPDATE_OK
  g_atrapa.otaTrwaMs = 40000;   // pobranie i zapis: 40 s bez powrotu do loop()
  const bool restart = otaSetupSkonczylSieRestartem();
  const Centrala c = kopia(0);
  info("zapytanie o status przed aktualizacja: %s", c.ostatniStatusSciezka.c_str());
  info("%s", liniaLogu("tag=OTA-PANEL zdarzenie=start").c_str());
  const bool ekranJednolity = otaEkranPusty();
  zrzutEkranu("w_czasie_pobierania");
  sprawdz(c.nStatus >= 1 && c.ostatniStatusSciezka.find("panel_fw=" WERSJA_FW) != std::string::npos && c.panelWersjaZgloszona == WERSJA_FW,
          "przed aktualizacja panel zglosil centrali swoja wersje (panel_fw=" WERSJA_FW ")");
  sprawdz(g_atrapa.otaWywolan == 1, "panel zaczal aktualizacje sam, przy starcie, raz");
  sprawdz(otaAdresPoprawny(), "adres: http://192.168.4.1:80/panel-ota.bin?token=<haslo OTA> (ten, ktory obsluguje centrala)");
  sprawdz(g_atrapa.otaRestartSam == 0, "biblioteka nie restartuje sama (rebootOnUpdate(false)) - restart robi firmware");
  sprawdz(!g_atrapa.otaWdtNadzorowal && g_atrapa.wdtZadzialal == 0,
          "na czas pobierania zadanie jest wypisane z watchdoga: 40 s bez powrotu do petli, watchdog (15 s) nie zadzialal");
  sprawdz(restart && g_atrapa.restarty == 1, "po udanym pobraniu panel restartuje sie");
  sprawdz(ekranJednolity, "STAN OBECNY: przez caly czas pobierania ekran jest pusty (ciemne tlo) - nie ma napisu o aktualizacji");
  sprawdz(logMa("tag=OTA-PANEL", "zdarzenie=start", std::string("wersja_docelowa=") + OTA_TAG_WYDANIA) && logMa("tag=OTA-PANEL", "zdarzenie=pobrano_ok"),
          "log: tag=OTA-PANEL zdarzenie=start z wersja docelowa, potem pobrano_ok");
  sprawdz(c.panelOtaDostepna && c.panelOtaPotwierdzen == 0, "centrala nadal czeka: flage zdejmie dopiero zgloszenie nowej wersji po restarcie");
}

// Pierwszy start po aktualizacji. Nowy firmware zglasza wersje IDENTYCZNA z
// tagiem wydania, na ktore czeka centrala.
void scenariuszOtaPoRestarcieZgodna() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  g_atrapa.powodResetu = 3;        // ESP_RST_SW: restart po udanym pobraniu
  g_atrapa.otaStanPartycji = 1;    // ESP_OTA_IMG_PENDING_VERIFY: pierwszy start nowego firmware'u
  otaCentralaCzekaNa(WERSJA_FW);   // tag wydania = to, co zglasza ten firmware
  g_atrapa.otaWynik = 2;           // gdyby panel mimo to zaczal pobierac drugi raz, bylby restart (blad scenariusza)
  setup();
  biegnij(6000);
  ustaw(0, [](Centrala& c) { c.tOgrz = 63.0; });
  biegnij(5000);
  const Centrala c = kopia(0);
  info("%s", liniaLogu("tag=OTA-ROLLBACK zdarzenie=potwierdzono_valid").c_str());
  info("centrala: potwierdzen=%d flaga=%d; panel: pobieran=%d restartow=%d", c.panelOtaPotwierdzen, (int)c.panelOtaDostepna, g_atrapa.otaWywolan, g_atrapa.restarty);
  sprawdz(g_atrapa.otaPotwierdzenPartycji == 1 && logMa("tag=OTA-ROLLBACK", "zdarzenie=potwierdzono_valid"),
          "nowy firmware potwierdzil swoja partycje (bez tego bootloader wrocilby przy nastepnym starcie do starej wersji)");
  sprawdz(c.panelOtaPotwierdzen == 1 && !c.panelOtaDostepna, "centrala dostala panel_fw rowne tagowi wydania i zdjela flage");
  sprawdz(g_atrapa.otaWywolan == 0, "panel nie pobiera drugi raz");
  sprawdz(c.ostatniStatusSciezka == "/api/status?panel_fw=" WERSJA_FW "&reset_powod=sw", "centrala dostaje zwykly powod startu po aktualizacji: reset_powod=sw");
  sprawdz(ekranMa("63°C"), "panel pracuje: kafelek PIEC pokazuje nowa wartosc z centrali (63°C)");
}

// STAN OBECNY. Pierwszy start po aktualizacji, ale nowy firmware zglasza INNY
// tekst niz tag wydania (dzis: panel "v1.3.0", wydania "3.57.9"). Centrala nie
// zdejmuje flagi, wiec panel robi dokladnie to samo co przed aktualizacja.
// Stan po tym starcie jest taki sam jak przed nim - to sie powtarza bez konca.
void scenariuszOtaPoRestarcieInnaWersja() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  g_atrapa.powodResetu = 3;        // ESP_RST_SW: restart po udanym pobraniu
  g_atrapa.otaStanPartycji = 1;    // ESP_OTA_IMG_PENDING_VERIFY
  otaCentralaCzekaNa(OTA_TAG_WYDANIA);
  g_atrapa.otaWynik = 2;
  g_atrapa.otaTrwaMs = 40000;
  const bool restart = otaSetupSkonczylSieRestartem();
  const Centrala c = kopia(0);
  info("panel zglosil: panel_fw=%s; centrala czeka na: %s; potwierdzen=%d flaga=%d", c.panelWersjaZgloszona.c_str(), c.panelOtaWersja.c_str(), c.panelOtaPotwierdzen, (int)c.panelOtaDostepna);
  info("pobieran w tym starcie: %d, restart: %d", g_atrapa.otaWywolan, (int)restart);
  sprawdz(g_atrapa.otaPotwierdzenPartycji == 1, "nowy firmware potwierdzil swoja partycje (aktualizacja sie udala)");
  sprawdz(c.panelWersjaZgloszona == WERSJA_FW && c.panelOtaPotwierdzen == 0 && c.panelOtaDostepna,
          "STAN OBECNY: zgloszona wersja rozni sie od tagu wydania - centrala NIE zdejmuje flagi");
  sprawdz(g_atrapa.otaWywolan == 1 && restart && g_atrapa.restarty == 1,
          "STAN OBECNY: panel pobiera ten sam firmware jeszcze raz i znow sie restartuje (petla przy kazdym starcie)");
  sprawdz(otaAdresPoprawny(), "adres pobierania ten sam co za pierwszym razem");
}

// STAN OBECNY. Panel pracuje, a centrala dopiero teraz konczy pobierac
// firmware z GitHuba i zaczyna zglaszac aktualizacje.
void scenariuszOtaFlagaWTrakciePracy() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  g_atrapa.otaWynik = 2;
  setup();
  biegnij(10000);
  sprawdz(g_atrapa.otaWywolan == 0, "bez flagi centrali panel niczego nie pobiera");
  otaCentralaCzekaNa(OTA_TAG_WYDANIA);
  const int zFlagaPrzed = kopia(0).nStatusZFlagaOta;
  biegnij(120000);
  const Centrala c = kopia(0);
  info("odpowiedzi centrali z \"panel_ota_dostepna\":true w ciagu 2 minut: %d; pobieran: %d", c.nStatusZFlagaOta - zFlagaPrzed, g_atrapa.otaWywolan);
  sprawdz(c.nStatusZFlagaOta - zFlagaPrzed >= 50, "przez 2 minuty panel dostawal od centrali flage aktualizacji (co 2 s)");
  sprawdz(g_atrapa.otaWywolan == 0 && g_atrapa.restarty == 0,
          "STAN OBECNY: pracujacy panel nie zaczyna aktualizacji - zrobi to dopiero po restarcie (wylaczenie i wlaczenie)");
  sprawdz(ekranMa("55°C"), "panel pracuje dalej: kafelek PIEC pokazuje wartosc z centrali");
}

// Pobieranie sie nie udaje (np. centrala nie ma pliku, zle haslo, zerwane
// polaczenie). Panel ma wrocic do zwyklej pracy.
void scenariuszOtaNieudane() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  otaCentralaCzekaNa(OTA_TAG_WYDANIA);
  g_atrapa.otaWynik = 0;        // HTTP_UPDATE_FAILED
  g_atrapa.otaTrwaMs = 20000;   // 20 s prob, zanim biblioteka sie podda
  const bool restart = otaSetupSkonczylSieRestartem();
  const int statusPrzed = kopia(0).nStatus;
  if (!restart) {
    biegnij(6000);
    ustaw(0, [](Centrala& c) { c.tOgrz = 63.0; });
    biegnij(60000);
  }
  const Centrala c = kopia(0);
  info("%s", liniaLogu("tag=OTA-PANEL lvl=ERR").c_str());
  info("pobieran: %d; zapytan o status po nieudanej probie (66 s): %d", g_atrapa.otaWywolan, c.nStatus - statusPrzed);
  sprawdz(!restart && g_atrapa.restarty == 0, "po nieudanym pobraniu panel sie nie restartuje");
  sprawdz(logMa("tag=OTA-PANEL", "lvl=ERR", "zdarzenie=nieudana"), "log: tag=OTA-PANEL lvl=ERR zdarzenie=nieudana z przyczyna");
  sprawdz(!g_atrapa.otaWdtNadzorowal && g_atrapa.wdtNadzoruje && g_atrapa.wdtZadzialal == 0, "watchdog: wypisany na czas proby, potem znow pilnuje petli; nie zadzialal");
  sprawdz(c.nStatus - statusPrzed >= 25 && ekranMa("63°C"), "panel wraca do zwyklej pracy: odpytuje centrale i pokazuje nowe wartosci (63°C)");
  sprawdz(g_atrapa.otaWywolan == 1, "w tej samej sesji nie ma drugiej proby (kolejna dopiero po nastepnym restarcie panelu)");
}

// STAN OBECNY. Firmware czeka na centrali, a panel startuje w chwili, gdy
// centrala zglasza alarm (np. po samoczynnym restarcie panelu w trakcie alarmu).
void scenariuszOtaPrzyAlarmie() {
  g_atrapa.eter.push_back(ap(BSSID_PIEC, 6, -45, 0));
  ustaw(0, [](Centrala& c) { c.tOgrz = 96.0; c.alarmOgrzewanie = true; });
  otaCentralaCzekaNa(OTA_TAG_WYDANIA);
  g_atrapa.otaWynik = 2;
  g_atrapa.otaTrwaMs = 40000;
  const int zmianTonuPrzed = g_atrapa.zmianTonu;
  const bool restart = otaSetupSkonczylSieRestartem();
  // Zwykly alarm: ton zmienia sie co kilkaset ms (1200/1600 Hz, patrz scenariusze alarmu).
  info("pobieran: %d, restart: %d; brzeczyk od wlaczenia do restartu (ponad 40 s): %d zmian tonu, na koncu %u Hz",
       g_atrapa.otaWywolan, (int)restart, g_atrapa.zmianTonu - zmianTonuPrzed, (unsigned)g_atrapa.tonHz);
  sprawdz(g_atrapa.otaWywolan == 1 && restart,
          "STAN OBECNY: aktualizacja zaczyna sie takze wtedy, gdy centrala zglasza alarm - panel nie sprawdza alarmu przed pobieraniem");
  sprawdz(g_atrapa.zmianTonu - zmianTonuPrzed <= 1,
          "STAN OBECNY: przez czas pobierania (tu 40 s) wzorzec alarmu nie gra - brzeczyk zostaje w stanie sprzed pobierania, petla nie pracuje");
  sprawdz(otaEkranPusty(), "STAN OBECNY: przez czas pobierania ekran jest pusty - ekranu alarmu nie widac");
}

struct Scenariusz {
  const char* nazwa;
  const char* opis;
  void (*funkcja)();
};
const Scenariusz SCENARIUSZE[] = {
    {"dwie_centrale", "dwie centrale o tej samej nazwie sieci (hipoteza 1 dla zgloszenia)", scenariuszDwieCentrale},
    {"dwie_centrale_piec_blizej", "dwie centrale, panel blizej tej przy piecu", scenariuszDwieCentralePiecBlizej},
    {"zla_odpowiedz", "centrala odpowiada, ale tresci nie da sie odczytac", scenariuszZlaOdpowiedz},
    {"brak_centrali_na_starcie", "panel wlaczony bez centrali w zasiegu", scenariuszBrakCentraliNaStarcie},
    {"zanik_i_powrot", "centrala znika w trakcie pracy i wraca", scenariuszZanikIPowrot},
    {"nieudany_post", "jedno nieudane polecenie z dotyku", scenariuszNieudanyPost},
    {"bateria_i_log", "minuta zwyklej pracy: objetosc logu, wskaznik baterii", scenariuszBateriaILog},
    {"skan_zawieszony", "skan WiFi, ktory sie nie konczy", scenariuszSkanZawieszony},
    {"ustawienia_milcza", "status dziala, /api/ustawienia-get bez odpowiedzi", scenariuszUstawieniaMilcza},
    {"gola_plytka", "centrala bez czujnikow + symulacja czujnika (stanowisko z 2026-11-11)", scenariuszGolaPlytka},
    {"martwe_polaczenie", "szybki restart centrali: WiFi \"polaczone\", centrala milczy (v1.2.1)", scenariuszMartwePolaczenie},
    {"autorestart_centrala_milczy", "centrala trwale nie odpowiada: restart panelu po 45 s (v1.2.1)", scenariuszAutorestartCentralaMilczy},
    {"autorestart_brak_wifi", "panel bez centrali w zasiegu: restart po 45 s (v1.2.1)", scenariuszAutorestartBrakWifi},
    {"start_po_autorestarcie", "start po samoczynnym restarcie: powod dla centrali (v1.2.1)", scenariuszStartPoAutorestarcie},
    {"seria_restartow", "piaty samoczynny restart w serii: centrala nie dostaje kolejnej wiadomosci (v1.2.1)", scenariuszSeriaRestartow},
    {"autorestart_kolejny", "drugi samoczynny restart z rzedu: dopiero po 90 s (v1.2.1)", scenariuszAutorestartKolejny},
    {"restart_po_ota", "restart programowy bez autorestartu i smieci w pamieci RTC (v1.2.1)", scenariuszRestartPoOta},
    {"zla_odpowiedz_dlugo", "centrala stale odpowiada nieczytelnie: bez rozlaczania WiFi, restart po 45 s (v1.2.1)", scenariuszZlaOdpowiedzDlugo},
    {"zla_odpowiedz_bez_konca", "nieczytelne odpowiedzi mimo dwoch restartow: trzeciego nie ma (v1.2.1)", scenariuszZlaOdpowiedzBezKonca},
    {"wycisz_alarm", "przycisk WYCISZ: cisza od razu, wyciszenie tylko do konca tego alarmu (v1.2.1)", scenariuszWyciszAlarm},
    {"autorestart_przy_alarmie", "centrala znika w trakcie alarmu: stan alarmu zapisany przed restartem (v1.2.1)", scenariuszAutorestartPrzyAlarmie},
    {"start_po_autorestarcie_z_alarmem", "start po restarcie w trakcie alarmu: dzwiek i ekran alarmu wracaja (v1.2.1)", scenariuszStartPoAutorestarcieZAlarmem},
    {"brak_pamieci_json", "panel nie ma pamieci na odczytanie odpowiedzi centrali - log z 2026-11-12 (v1.2.2)", scenariuszBrakPamieciJson},
    {"brak_pamieci_odbior", "panel nie ma pamieci nawet na odebranie odpowiedzi: to nie jest \"zla odpowiedz\" (v1.2.2)", scenariuszBrakPamieciOdbior},
    {"brak_pamieci_lvgl", "LVGL nie dostaje pamieci w trakcie rysowania: restart od razu, z powodem (v1.2.2)", scenariuszBrakPamieciLvgl},
    {"wykresy_pamiec", "bufory ekranu Wykresy istnieja tylko wtedy, gdy ekran jest otwarty (v1.2.2)", scenariuszWykresyPamiec},
    {"wykresy_centrala_liczy", "wykres z gotowych punktow centrali 3.58.4: GET /api/history/panel (v1.3.0)", scenariuszWykresyCentralaLiczy},
    {"wykresy_stara_centrala", "centrala <= 3.58.3 bez /api/history/panel: wykres po staremu, taki sam jak w v1.2.2 (v1.3.0)", scenariuszWykresyStaraCentrala},
    {"wykresy_przerwy", "przerwa w pracy centrali i czujnik bez odczytu: linia przerwana, nie dociagnieta (v1.3.0)", scenariuszWykresyPrzerwy},
    {"wykresy_brak_danych", "centrala bez historii, jeden pomiar, bez zegara, brak pamieci centrali (v1.3.0)", scenariuszWykresyBrakDanych},
    {"wykresy_bledy_sieci", "odpowiedz urwana, cisza, brak odpowiedzi, nie-centrala, brak WiFi: napis z przyczyna (v1.3.0)", scenariuszWykresyBledySieci},
    {"wykresy_wyjscie_w_trakcie", "wyjscie z wykresu w trakcie pobierania; pobieranie dluzsze niz limit (v1.3.0)", scenariuszWykresyWyjscieWTrakcie},
    {"wykresy_alarm_w_trakcie", "alarm w trakcie pobierania danych do wykresu (v1.3.0)", scenariuszWykresyAlarmWTrakcie},
    {"wykresy_stary_csv", "stary CSV podany ekranowi wprost: te same punkty co w v1.2.2 (v1.3.0)", scenariuszWykresyStaryCsv},
    {"wykresy_po_restarcie_centrali", "centrala 3.58.5 tuz po restarcie: same przerwy -> napis; przerwa do konca osi (v1.3.0 bez zmian)", scenariuszWykresyPoRestarcieCentrali},
    {"ota_start", "OTA: centrala ma gotowy firmware panelu - panel przy starcie pobiera go i restartuje sie", scenariuszOtaStart},
    {"ota_po_restarcie_zgodna", "OTA: pierwszy start po aktualizacji, zgloszona wersja = tag wydania - centrala zdejmuje flage, panel pracuje", scenariuszOtaPoRestarcieZgodna},
    {"ota_po_restarcie_inna_wersja", "OTA, STAN OBECNY: zgloszona wersja inna niz tag wydania - panel pobiera i restartuje sie przy kazdym starcie", scenariuszOtaPoRestarcieInnaWersja},
    {"ota_flaga_w_trakcie_pracy", "OTA, STAN OBECNY: pracujacy panel nie zaczyna aktualizacji, dopoki sie nie zrestartuje", scenariuszOtaFlagaWTrakciePracy},
    {"ota_nieudane", "OTA: pobieranie nieudane - panel pracuje dalej, kolejna proba po nastepnym restarcie", scenariuszOtaNieudane},
    {"ota_przy_alarmie", "OTA, STAN OBECNY: aktualizacja zaczyna sie takze w trakcie alarmu", scenariuszOtaPrzyAlarmie},
#ifdef POMIAR_STERTY
    {"pomiar_pamieci", "ile pamieci trzyma firmware i kto ja wzial (tylko pomiar_pamieci.sh)", scenariuszPomiarPamieci},
    {"pomiar_przypadki", "ile RAM-u potrzeba w kazdym przypadku pracy (tylko pomiar_pamieci.sh --przypadki)", scenariuszPomiarPrzypadki},
    {"pomiar_malo_ramu", "co robi panel, gdy wolnego RAM-u jest LCD_WOLNY_RAM_B bajtow (tylko pomiar_pamieci.sh --przypadki)", scenariuszPomiarMaloRamu},
#endif
};

// Scenariusze, w ktorych ESP.restart() JEST oczekiwany; w pozostalych kazdy
// restart to blad (sprawdzane w main()).
bool restartOczekiwany(const char* nazwa) {
  const std::string n = nazwa;
  return n == "autorestart_centrala_milczy" || n == "autorestart_brak_wifi" || n == "autorestart_kolejny" || n == "zla_odpowiedz_dlugo" ||
         n == "autorestart_przy_alarmie" || n == "zla_odpowiedz_bez_konca" || n == "brak_pamieci_json" || n == "brak_pamieci_lvgl" ||
         n == "pomiar_malo_ramu" || n == "ota_start" || n == "ota_po_restarcie_inna_wersja" || n == "ota_przy_alarmie";
}

}  // namespace

// ===========================================================================
// Przekierowanie gniazd panelu (linker: -Wl,--wrap=connect,--wrap=send)
// ===========================================================================
extern "C" int __real_connect(int fd, const struct sockaddr* adres, socklen_t dlugosc);
extern "C" ssize_t __real_send(int fd, const void* bufor, size_t n, int flagi);

// Panel laczy sie zawsze z 192.168.4.1:80. Tutaj: jesli "radio" nie jest
// polaczone z zadnym AP - odmowa od razu (na plytce lwIP tez nie ma wtedy
// trasy); w przeciwnym razie polaczenie idzie do serwera testowego, ktory
// odpowie jako centrala stojaca za tym AP.
extern "C" int __wrap_connect(int fd, const struct sockaddr*, socklen_t) {
  // Kolejka ponizej to pamiec testu, nie firmware'u - nie wolno jej liczyc w
  // pomiarze pamieci (pomiar_pamieci.sh) ani odmawiac jej w scenariuszach.
  AtrapaBezPomiaru bezPomiaru;
  struct BezOdmowy {
    int bylo = pamiec::wFirmware;
    BezOdmowy() { pamiec::wFirmware = 0; }
    ~BezOdmowy() { pamiec::wFirmware = bylo; }
  } bezOdmowy;
  const AtrapaAp* apTeraz = g_atrapa.polaczonyAp();
  if (apTeraz == nullptr) {
    errno = EHOSTUNREACH;
    return -1;
  }
  struct sockaddr_in lokalny;
  memset(&lokalny, 0, sizeof(lokalny));
  lokalny.sin_family = AF_INET;
  lokalny.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bool odmawia;
  {
    std::lock_guard<std::mutex> blokada(g_mutex);
    odmawia = (g_centrale[apTeraz->centrala].trybStatus == TRYB_ODMAWIA);
    // -1: radio "polaczone", ale centrala zrestartowala sie i nie zna panelu
    if (!odmawia) g_kolejkaCentral.push_back(g_atrapa.polaczenieZywe() ? apTeraz->centrala : -1);
  }
  lokalny.sin_port = htons((uint16_t)(odmawia ? g_portZamkniety : g_portSerwera));
  const int r = __real_connect(fd, (struct sockaddr*)&lokalny, sizeof(lokalny));
  if (r < 0 && errno == EINPROGRESS) {
    // Zegar "plytki" jest sztuczny i biegnie duzo szybciej niz prawdziwy:
    // 2 s limitu laczenia w firmware to kilka milisekund pracy tego programu.
    // Jadro konczy nawiazywanie polaczenia lokalnego zwykle od razu, ale na
    // obciazonej maszynie potrafi to odlozyc o kilkadziesiat milisekund - a
    // wtedy panel widzialby "timeout laczenia", ktorego scenariusz nie
    // przewiduje (zaobserwowane: ok. 1 na 60 przebiegow). Dlatego tutaj
    // prawdziwe czekanie, az polaczenie powstanie albo zostanie odrzucone.
    // Panel i tak dostaje EINPROGRESS i sprawdza wynik po swojemu.
    struct pollfd p;
    p.fd = fd;
    p.events = POLLOUT;
    p.revents = 0;
    poll(&p, 1, 2000);
    errno = EINPROGRESS;
  }
  return r;
}

extern "C" ssize_t __wrap_send(int fd, const void* bufor, size_t n, int flagi) {
  const ssize_t r = __real_send(fd, bufor, n, flagi | MSG_NOSIGNAL);
  if (r > 0) g_wLocie++;   // panel wyslal zapytanie - serwer ma na nie zareagowac (patrz hakCzasu())
  return r;
}

int main(int argc, char** argv) {
  if (pomiar_wstrzymaj != nullptr) pomiar_wstrzymaj(1);   // od tej chwili liczy sie tylko firmware (LiczFirmware)
  const Scenariusz* wybrany = nullptr;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--lista") {
      for (const Scenariusz& s : SCENARIUSZE) printf("%s\n", s.nazwa);
      return 0;
    } else if (a == "--obserwacje") {
      g_obserwacje = true;
    } else if (a == "--log") {
      g_atrapa.echoSerial = true;
    } else if (a == "--wyjscie" && i + 1 < argc) {
      g_katalogWyjscia = argv[++i];
      mkdir(g_katalogWyjscia.c_str(), 0755);
    } else {
      for (const Scenariusz& s : SCENARIUSZE)
        if (a == s.nazwa) wybrany = &s;
      if (wybrany == nullptr) { fprintf(stderr, "nieznany scenariusz: %s (zobacz --lista)\n", a.c_str()); return 2; }
    }
  }
  if (wybrany == nullptr) { fprintf(stderr, "podaj nazwe scenariusza albo --lista\n"); return 2; }

  g_nazwaScenariusza = wybrany->nazwa;
  printf("== %s: %s\n", wybrany->nazwa, wybrany->opis);
  uruchomSerwer();
  atrapa_hak_czasu = hakCzasu;
  plytkaDomyslna();
  try {
    wybrany->funkcja();
  } catch (const AtrapaRestart&) {
    // ESP.restart() wywolany poza loop() (np. z setup()) - scenariusz konczy sie tutaj.
    g_poRestarcie = true;
  }
  if (!restartOczekiwany(wybrany->nazwa)) sprawdz(g_atrapa.restarty == 0, "panel nie zrestartowal sie sam w tym scenariuszu");
  zapiszLog();
  g_koniec = true;

  if (g_obserwacje) {
    printf("   (tryb obserwacji: bez oceny)\n");
    return 0;
  }
  printf("   %s: %d/%d sprawdzen OK\n", g_bledy == 0 ? "ZALICZONY" : "NIEZALICZONY", g_sprawdzen - g_bledy, g_sprawdzen);
  return g_bledy == 0 ? 0 : 1;
}
