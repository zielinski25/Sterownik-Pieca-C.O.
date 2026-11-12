// ============================================================================
// atrapa.h - stan "plytki na PC" wspolny dla wszystkich atrap w tym katalogu
// ============================================================================
// Atrapy (Arduino.h, WiFi.h, TFT_eSPI.h, Preferences.h, esp_*.h ...) pozwalaja
// skompilowac i URUCHOMIC prawdziwy src/main.cpp panelu zwyklym g++ na PC:
// setup() i loop() dzialaja jak na plytce, tylko sprzet jest udawany przez
// zmienne ponizej. Test (symulator_panelu.cpp) ustawia je i odczytuje.
//
// To NIE jest emulator ESP32. Modelowane jest tylko to, od czego zalezy
// logika panelu: czas (millis/delay), watchdog zadania, WiFi (lista sieci w
// eterze, skan, laczenie), NVS, wejscia GPIO/ADC i bufor ramki ekranu.
// ============================================================================
#pragma once

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

struct AtrapaAp {              // jeden punkt dostepowy "w eterze"
  std::string ssid;
  uint8_t bssid[6];
  int kanal;
  int rssi;                    // dBm
  int centrala;                // ktora centrala testowa odpowiada za tym AP (indeks w tescie)
  // Numer "wcielenia" nadajnika. Test zwieksza go, zeby udac SZYBKI restart
  // centrali: nadajnik wraca, zanim radio panelu zauwazy przerwe, wiec panel
  // dalej uwaza sie za polaczony - ale centrala po restarcie go nie zna i
  // nie odpowiada mu, dopoki panel nie polaczy sie od nowa (WiFi.begin()).
  int wcielenie = 0;
};

struct AtrapaRestart {};       // rzucany przez ESP.restart() - patrz Arduino.h

// Pomiar pamieci (pomiar_pamieci.sh, pomiar_sterty.c): to, co atrapy same
// odkladaja na sterycie (zapis Seriala, tablice NVS, wyniki skanu), nie jest
// pamiecia firmware'u i nie moze byc do niej liczone. W zwyklych testach tej
// funkcji nie ma (slaby symbol = nullptr) i straznik nic nie robi.
extern "C" void pomiar_wstrzymaj(int o_ile) __attribute__((weak));
struct AtrapaBezPomiaru {
  AtrapaBezPomiaru() { if (pomiar_wstrzymaj != nullptr) pomiar_wstrzymaj(1); }
  ~AtrapaBezPomiaru() { if (pomiar_wstrzymaj != nullptr) pomiar_wstrzymaj(-1); }
  AtrapaBezPomiaru(const AtrapaBezPomiaru&) = delete;
  AtrapaBezPomiaru& operator=(const AtrapaBezPomiaru&) = delete;
};

struct Atrapa {
  // --- czas: millis() zwraca terazMs; delay(ms) przesuwa zegar o ms ---
  unsigned long terazMs = 0;

  // --- Serial: wszystko, co firmware wypisal ---
  std::string serial;
  bool echoSerial = false;     // true = dodatkowo na stdout, z czasem [ms]

  // --- watchdog zadania (esp_task_wdt_*) ---
  // Model: gdy zadanie jest pod nadzorem i od ostatniego esp_task_wdt_reset()
  // minelo wiecej niz timeout, prawdziwy ESP32 zrobilby panic + restart.
  // Atrapa tylko to ODNOTOWUJE (licznik + chwila pierwszego zadzialania).
  bool wdtNadzoruje = false;
  uint32_t wdtTimeoutMs = 0;
  unsigned long wdtOstatnieKarmienieMs = 0;
  int wdtZadzialal = 0;
  unsigned long wdtPierwszyRazMs = 0;

  // --- GPIO / ADC ---
  int gpio[64];                // digitalRead(pin)
  int adcMv[64];               // analogReadMilliVolts(pin)

  // --- WiFi ---
  std::vector<AtrapaAp> eter;  // sieci, ktore "nadaja"
  int metodaSkanu = 0;         // 0 = WIFI_FAST_SCAN (domyslna w arduino-esp32), 1 = WIFI_ALL_CHANNEL_SCAN
  int metodaSortowania = 0;    // 0 = WIFI_CONNECT_AP_BY_SIGNAL
  unsigned long czasLaczeniaMs = 1200;   // ile trwa begin() -> WL_CONNECTED
  unsigned long czasSkanuMs = 1800;      // ile trwa skan wszystkich kanalow
  bool skanZawieszony = false;           // true = skan nigdy sie nie konczy (test limitu czasu)
  bool celUstawiony = false;             // czy begin() znalazl pasujacy AP
  uint8_t celBssid[6] = {0};
  int celWcielenie = 0;                  // wcielenie AP w chwili begin() - patrz AtrapaAp::wcielenie
  unsigned long polaczenieOdMs = 0;
  bool skanTrwa = false;
  unsigned long skanKoniecMs = 0;
  std::vector<AtrapaAp> wynikSkanu;
  int liczbaBegin = 0;         // ile razy firmware wolal WiFi.begin()
  int liczbaBeginZBssid = 0;   // ...w tym z podanym BSSID
  int liczbaBeginZBssidPelnySkan = 0;   // ...w tym z BSSID, ale przy WIFI_ALL_CHANNEL_SCAN (niepotrzebny pelny skan przed kazdym laczeniem)
  int liczbaSkanow = 0;        // ile razy firmware wolal WiFi.scanNetworks()
  int liczbaStopSkanu = 0;     // esp_wifi_scan_stop()
  int liczbaDisconnect = 0;    // WiFi.disconnect()
  // WiFi.onEvent(): funkcja firmware'u wolana przy rozlaczeniu. Atrapa wola
  // ja z kodem 8 (ASSOC_LEAVE) przy WiFi.disconnect() w stanie polaczonym;
  // test moze zglosic dowolny kod przez zglosRozlaczenie().
  void (*naRozlaczenie)(int kod) = nullptr;
  void zglosRozlaczenie(int kod) { if (naRozlaczenie != nullptr) naRozlaczenie(kod); }

  // --- NVS (Preferences): przestrzen -> klucz -> wartosc ---
  std::map<std::string, std::map<std::string, long long>> nvs;
  std::map<std::string, std::map<std::string, std::string>> nvsTeksty;

  // --- ekran: bufor ramki 480x320 RGB565 (to, co poszlo przez TFT_eSPI) ---
  uint16_t ekran[480 * 320];
  std::vector<std::string> napisyTft;    // teksty rysowane tft.drawString()

  // --- brzeczyk (ledcWriteTone) ---
  uint32_t tonHz = 0;          // ostatnio ustawiona czestotliwosc; 0 = cisza
  int zmianTonu = 0;           // ile razy czestotliwosc sie zmienila (wzorzec alarmu "gra")

  // --- pamiec (esp_heap_caps.h): to, co firmware odczytuje jako wolny RAM ---
  // Zwykle testy ustawiaja te liczby recznie; pomiar_pamieci.sh podstawia
  // tu wynik licznika sterty (patrz pomiar_sterty.c).
  size_t ramWolny = 52000;     // heap_caps_get_free_size()
  size_t ramBlok = 40000;      // heap_caps_get_largest_free_block()
  size_t ramMin = 45000;       // heap_caps_get_minimum_free_size()
  // Serial.printf() na plytce prosi o pamiec dla tekstu od 64 znakow; true =
  // tej pamieci nie dostaje i taka linia przepada (patrz AtrapaSerial::printf).
  bool serialBezPamieci = false;
  int serialLiniiUtraconych = 0;
  std::string serialOstatniaUtracona;   // poczatek ostatniej takiej linii (do raportu testu)

  // --- inne ---
  int restarty = 0;            // ESP.restart()
  int powodResetu = 1;         // esp_reset_reason(): 1 = ESP_RST_POWERON

  Atrapa();
  const AtrapaAp* polaczonyAp() const;   // AP, z ktorym "radio" jest teraz polaczone (albo nullptr)
  // true = centrala za tym AP zna panel (nie restartowala sie od chwili polaczenia).
  bool polaczenieZywe() const;
};

// Stan jest tworzony przy PIERWSZYM uzyciu (a nie jako zwykla zmienna
// globalna), bo firmware ma wlasne obiekty globalne, ktorych konstruktory
// moga siegnac do atrapy, zanim C++ zdazylby ja zbudowac (kolejnosc
// inicjalizacji miedzy plikami .cpp nie jest okreslona).
Atrapa& atrapa_stan();
static Atrapa& g_atrapa = atrapa_stan();

// Wolane przez delay() PO przesunieciu zegara - test moze tu oddac czas
// watkowi serwera HTTP (prawdziwe gniazda potrzebuja prawdziwych mikrosekund).
extern void (*atrapa_hak_czasu)(unsigned long ms);

// Przesuwa zegar i sprawdza watchdog. delay() wola to samo.
void atrapa_uplyw_czasu(unsigned long ms);
