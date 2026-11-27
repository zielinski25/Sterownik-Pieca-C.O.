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
struct AtrapaSen {};           // [v1.5.0] rzucany przez esp_deep_sleep_start() - patrz esp_sleep.h

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

  // --- obciazenie (pomiar_obciazenia.sh): ile poszlo do ekranu i ile razy zmieniono jasnosc ---
  // Tylko liczniki - zaden zwykly scenariusz od nich nie zalezy.
  unsigned long long spiPikseli = 0;       // suma pikseli oddanych do pushColors() (kazdy to 3 bajty po SPI przy ILI9488)
  unsigned long spiOkien = 0;              // ile razy LVGL otworzyl okno rysowania (setAddrWindow) = liczba paskow/fragmentow
  unsigned long podswietlenieZapisow = 0;  // ile razy firmware ustawil jasnosc podswietlenia (ledcWrite na pinie TFT_BL)
  long podswietlenieWypelnienie = -1;      // ostatnio ustawione wypelnienie PWM (0..255); -1 = jeszcze nie ustawiono

  // --- brzeczyk (ledcWriteTone - pasywny; od v1.5.19 ledcWrite(13, 255|0) - aktywny) ---
  uint32_t tonHz = 0;          // ostatnio ustawiony ton: 0 = cisza; pasywny: Hz; aktywny: 255 = gra
  int zmianTonu = 0;           // ile razy ton sie zmienil (wzorzec alarmu "gra")

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
  bool wifiPersistent = true;  // WiFi.persistent(): true = biblioteka zapisuje siec do NVS (domyslne w rdzeniu 3.x)
  // [v1.4.7] taktowanie CPU (esp32-hal-cpu.h) i moc nadajnika (WiFi.setTxPower()).
  uint32_t cpuMhz = 240;       // getCpuFrequencyMhz(); plytka startuje z 240 MHz (platformio: esp32dev, bez board_build.f_cpu)
  int cpuZmian = 0;            // ile razy firmware wolal setCpuFrequencyMhz()
  int cpuZleWartosci = 0;      // ...w tym z wartoscia spoza 240/160/80 (radio by padlo)
  int wifiTryb = 0;            // WiFi.mode(): 0 = WIFI_OFF (sterownik radia nie wystartowal), 1 = WIFI_STA
  int txMoc = 78;              // esp_wifi_set_max_tx_power(): cwiartki dBm, domyslnie 78 = 19,5 dBm
  // [v1.5.0] gleboki sen (esp_sleep.h) i "hold" pinow (driver/gpio.h).
  int usnal = 0;                       // ile razy firmware wolal esp_deep_sleep_start()
  unsigned long usnalMs = 0;           // chwila ostatniego usniecia
  int snuExt0Pin = -1;                 // esp_sleep_enable_ext0_wakeup(): pin (albo -1 = nie ustawiono)
  int snuExt0Poziom = -1;              // ...i poziom budzacy
  unsigned long long snuExt1Maska = 0; // esp_sleep_enable_ext1_wakeup(): maska pinow (0 = nie ustawiono)
  int snuExt1Tryb = -1;                // ...i tryb (0 = ALL_LOW, 1 = ANY_HIGH)
  bool pinTrzymany[64] = {false};      // gpio_hold_en()/gpio_hold_dis(): czy stan pinu jest "zamrozony"
  int wybudzenie = 0;                  // esp_sleep_get_wakeup_cause(): 0 = zwykly start, 2 = EXT0 (przycisk), 3 = EXT1 (ladowarka)
  int txUstawien = 0;          // udane WiFi.setTxPower()
  int txPrzedStartem = 0;      // WiFi.setTxPower() PRZED WiFi.mode() - na plytce zwraca blad i nic nie ustawia
  unsigned stosZapasB = 5000;  // uxTaskGetStackHighWaterMark(NULL) [B]
  void (*hakNieudanejAlokacji)(size_t, uint32_t, const char*) = nullptr;   // heap_caps_register_failed_alloc_callback()
  // --- zrzut po awarii (esp_core_dump.h) ---
  // zrzutJest=true: esp_core_dump_image_check() zwraca ESP_OK, a skrot ma
  // ponizsze wartosci; po esp_core_dump_image_erase() zrzutJest = false.
  bool zrzutJest = false;
  bool zrzutUszkodzony = false;          // true: image_check() zwraca ESP_ERR_INVALID_CRC
  int zrzutSkasowan = 0;
  const char* zrzutZadanie = "loopTask";
  uint32_t zrzutPc = 0x400d1234u;
  const char* zrzutPowod = "LoadProhibited";
  uint32_t zrzutSlad[16] = {0x400d1234u, 0x400d5678u, 0x400e0001u};
  uint32_t zrzutSladN = 3;

  // --- aktualizacja OTA panelu (HTTPUpdate.h, esp_ota_ops.h) ---
  // httpUpdate.update() atrapy niczego nie pobiera i niczego nie zapisuje:
  // odnotowuje adres, przesuwa zegar o otaTrwaMs (na plytce pobranie i zapis
  // do flash trwaja, a zadanie nie wraca w tym czasie do loop()) i zwraca
  // otaWynik. Stan partycji to to, co firmware odczyta przy starcie.
  int otaWywolan = 0;                 // ile razy firmware wolal httpUpdate.update()
  std::string otaAdres;               // adres z ostatniego wywolania
  int otaWynik = 0;                   // 0 = HTTP_UPDATE_FAILED (domyslnie), 1 = HTTP_UPDATE_NO_UPDATES, 2 = HTTP_UPDATE_OK
  unsigned long otaTrwaMs = 0;        // czas plytki, ktory zajmuje jedno update()
  bool otaWdtNadzorowal = false;      // czy watchdog pilnowal zadania w chwili wywolania update()
  int otaRestartSam = -1;             // httpUpdate.rebootOnUpdate(): -1 = nie wolane, 0 = false, 1 = true
  int otaStanPartycji = 2;            // esp_ota_get_state_partition(): 2 = VALID, 1 = PENDING_VERIFY (pierwszy start po OTA), -1 = wgrane przez USB
  int otaPotwierdzenPartycji = 0;     // ile razy firmware wolal esp_ota_mark_app_valid_cancel_rollback()

  // --- aktualizacja OTA od firmware'u v1.4.0 (Update.h, esp_partition_get_sha256()) ---
  // Pola wyzej (httpUpdate) zostaja dla starszych zrodel (SRC=...); v1.4.0 ich nie uzywa.
  uint8_t otaSumaWlasna[32];          // suma SHA-256 programu, z ktorego "plytka" pracuje (konstruktor: a1 a2 a3 ...)
  bool otaSumaBlad = false;           // true: sumy wlasnego programu nie da sie odczytac
  size_t otaPartycjaB = 0x1F0000;     // rozmiar partycji na nowy program (partitions.csv: app1)
  bool otaBeginBezPamieci = false;    // Update.begin() nie dostaje pamieci na bufor
  long otaBladZapisuPo = -1;          // >= 0: Update.write() zawodzi po tylu zapisanych bajtach
  unsigned long otaMsNaSektor = 15;   // czas plytki na kazdy zapisany sektor 4096 B (kasowanie + zapis)
  std::vector<uint8_t> otaZapisane;   // to, co firmware oddal do Update.write()
  int otaBeginWywolan = 0;            // ile razy firmware zaczynal zapis (Update.begin())
  int otaUruchomiono = 0;             // ile razy Update.end() ustawil nowy program jako startowy
  int otaPorzucono = 0;               // ile razy rozpoczety zapis zostal porzucony (abort albo blad)
  int otaVscodeBegin = 0;             // [v1.5.4] ArduinoOTA.begin() - wgrywanie z PlatformIO przez WiFi
  long otaVscodeHandle = 0;           // [v1.5.4] ArduinoOTA.handle() (kazdy obrot petli po begin())
  unsigned long otaVscodeMsNaKawalek = 10;   // [v1.5.4] czas plytki na kazdy kawalek 1460 B (odbior + zapis do flash)
  bool nvsZapisOdmawia = false;       // Preferences::putBytes() zwraca 0 (NVS pelne albo uszkodzone)
  int nvsZapisowBinarnych = 0;        // udane putBytes()

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
