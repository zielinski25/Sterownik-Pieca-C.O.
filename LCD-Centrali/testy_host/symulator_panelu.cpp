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
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <lvgl.h>

#include "Arduino.h"
#include "WiFi.h"

// --- z firmware'u (src/main.cpp) ---
void setup();
void loop();
// --- z dashboardu (src/dashboard_lvgl_poc.cpp); w v1.1.0 tej funkcji nie ma ---
void lvgl_dashboard_poc_aktualizuj_centrale(const char* znacznik) __attribute__((weak));
void pompaManualWyslij(const char* stanTekst);

namespace {

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
  unsigned symulacja = 0;   // suma SYM_*: czujniki, ktore centrala podaje jako symulowane
  // liczniki zapytan, ktore do tej centrali dotarly
  int nStatus = 0, nUstawienia = 0, nPost = 0, nInne = 0;
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
  j += "\"dym\":" + std::to_string(c.dym) + ",\"dym_swiezy\":" + tn(c.dymWlaczony) + ",\"dym_wlaczony\":" + tn(c.dymWlaczony) + ",\"dym_alarm\":false,";
  j += "\"alarm_dym_wyciszony\":false,\"alarm_dym_wyciszenie_ms\":0,\"alarm_ogrz_wyciszony\":false,\"alarm_ogrz_wyciszenie_ms\":0,";
  j += "\"alarm_sound_dym\":\"syrena\",\"alarm_sound_ogrz\":\"pulsujacy\",\"alarm_sound_both\":\"syrena\",";
  j += "\"klapa\":" + std::to_string(c.klapa) + ",\"syberka\":" + std::to_string(c.syberka) + ",\"tryb_serwa\":1,";
  j += std::string("\"pompa\":") + tn(c.pompa) + ",\"pompa_ms_od_zmiany\":1049000,\"pompa_override_min\":0,";
  j += "\"wybor\":1,\"mieszadlo\":false,\"rozpalanie\":false,\"rozpalanie_potwierdzone\":false,\"rozpalanie_pozostalo_s\":0,";
  j += "\"alarm_ogrzewanie\":false,\"alarm_panel\":false,";
  j += "\"rozpalanie_serwa_dostepne\":true,\"rozpalanie_serwa_aktywne\":false,\"rozpalanie_serwa_blokada_dymu\":false,";
  j += "\"rozpalanie_serwa_temp_ok\":true,\"rozpalanie_serwa_alarm_bezpieczenstwa\":false,";
  j += "\"miesiac\":" + std::to_string(c.rtcOk ? c.miesiac : 0) + ",\"godzina\":" + std::to_string(c.rtcOk ? c.godzina : 0);
  j += ",\"minuta\":" + std::to_string(c.rtcOk ? c.minuta : 0) + ",\"dzien\":" + std::to_string(c.rtcOk ? c.dzien : 0);
  j += ",\"rok\":" + std::to_string(c.rtcOk ? c.rok : 0) + ",\"rtc_ok\":" + tn(c.rtcOk) + ",\"internet\":false,";
  j += "\"panel_ota_dostepna\":false,\"panel_ota_wersja\":\"\"";
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
      tryb = c.trybStatus;
      cialo = jsonStatus(c, tryb == TRYB_NAN);
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

void watekSerwera() {
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
    obsluzPolaczenie(k, idx);
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
void hakCzasu(unsigned long) {
  for (int i = 0; i < 4000 && g_wLocie.load() > 0; i++) usleep(50);   // najwyzej 0,2 s prawdziwego czasu
}

// ===========================================================================
// Narzedzia scenariuszy
// ===========================================================================
bool g_obserwacje = false;
int g_bledy = 0, g_sprawdzen = 0;
std::string g_katalogWyjscia;
std::string g_nazwaScenariusza;

void biegnij(unsigned long ms) {
  const unsigned long koniec = millis() + ms;
  while (millis() < koniec) loop();
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
  sprawdz(logMa("tag=PANEL fw=v1.2.0", "bssid=24:6F:28:11:22:33", "centrala=sterownik", od), "linia tag=PANEL podaje wersje, BSSID i rodzaj centrali");
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
    lvgl_dashboard_poc_aktualizuj_centrale("");
  } else {
    sprawdz(false, "najdluzszy znacznik miesci sie w naglowku (x od 80 do 395)");
  }
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
};

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
    if (!odmawia) g_kolejkaCentral.push_back(apTeraz->centrala);
  }
  lokalny.sin_port = htons((uint16_t)(odmawia ? g_portZamkniety : g_portSerwera));
  return __real_connect(fd, (struct sockaddr*)&lokalny, sizeof(lokalny));
}

extern "C" ssize_t __wrap_send(int fd, const void* bufor, size_t n, int flagi) {
  const ssize_t r = __real_send(fd, bufor, n, flagi | MSG_NOSIGNAL);
  if (r > 0) g_wLocie++;   // panel wyslal zapytanie - serwer ma na nie zareagowac (patrz hakCzasu())
  return r;
}

int main(int argc, char** argv) {
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
  wybrany->funkcja();
  zapiszLog();
  g_koniec = true;

  if (g_obserwacje) {
    printf("   (tryb obserwacji: bez oceny)\n");
    return 0;
  }
  printf("   %s: %d/%d sprawdzen OK\n", g_bledy == 0 ? "ZALICZONY" : "NIEZALICZONY", g_sprawdzen - g_bledy, g_sprawdzen);
  return g_bledy == 0 ? 0 : 1;
}
