// ============================================================================
// atrapa.cpp - zachowanie atrap (patrz atrapa.h)
// ============================================================================
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
#include "HTTPUpdate.h"
#include "WiFi.h"

Atrapa& atrapa_stan() {
  AtrapaBezPomiaru bezPomiaru;
  static Atrapa* stan = new Atrapa();   // celowo nigdy nie zwalniany: watek serwera testowego moze go czytac do konca procesu
  return *stan;
}
AtrapaSerial Serial;
AtrapaEsp ESP;
AtrapaWiFi WiFi;
AtrapaHttpUpdate httpUpdate;
void (*atrapa_hak_czasu)(unsigned long ms) = nullptr;

Atrapa::Atrapa() {
  for (int i = 0; i < 64; i++) { gpio[i] = HIGH; adcMv[i] = 0; }
  memset(ekran, 0, sizeof(ekran));
  for (int i = 0; i < 32; i++) otaSumaWlasna[i] = (uint8_t)(0xA1 + i);   // identyfikator obrazu: "a1a2a3a4a5a6a7a8"
}

const AtrapaAp* Atrapa::polaczonyAp() const {
  if (!celUstawiony || terazMs < polaczenieOdMs) return nullptr;
  for (const AtrapaAp& ap : eter)
    if (memcmp(ap.bssid, celBssid, 6) == 0) return &ap;
  return nullptr;   // AP zniknal z eteru -> polaczenie zerwane
}

bool Atrapa::polaczenieZywe() const {
  const AtrapaAp* ap = polaczonyAp();
  return ap != nullptr && ap->wcielenie == celWcielenie;
}

void atrapa_uplyw_czasu(unsigned long ms) {
  g_atrapa.terazMs += ms;
  if (g_atrapa.wdtNadzoruje && g_atrapa.wdtTimeoutMs > 0 &&
      g_atrapa.terazMs - g_atrapa.wdtOstatnieKarmienieMs > g_atrapa.wdtTimeoutMs) {
    // Prawdziwy ESP32: "task_wdt: Task watchdog got triggered" + panic + restart.
    if (g_atrapa.wdtZadzialal == 0) g_atrapa.wdtPierwszyRazMs = g_atrapa.terazMs;
    g_atrapa.wdtZadzialal++;
    g_atrapa.wdtOstatnieKarmienieMs = g_atrapa.terazMs;   // licz kolejne okresy osobno
  }
  if (atrapa_hak_czasu != nullptr) atrapa_hak_czasu(ms);
}

// ---------------------------------------------------------------------------
// Serial
// ---------------------------------------------------------------------------
void AtrapaSerial::dopisz(const char* s) {
  AtrapaBezPomiaru bezPomiaru;
  g_atrapa.serial += s;
  if (g_atrapa.echoSerial) {
    static bool poczatekLinii = true;
    // "::" jest tu konieczne: samo printf() w metodzie tej klasy oznaczaloby
    // AtrapaSerial::printf(), czyli nieskonczona rekurencje.
    for (const char* p = s; *p; p++) {
      if (poczatekLinii) ::printf("[%8lu ms] ", g_atrapa.terazMs);
      ::putchar(*p);
      poczatekLinii = (*p == '\n');
    }
    ::fflush(stdout);
  }
}

// Jak Print::printf() w arduino-esp32: tekst do 63 znakow powstaje w buforze
// na stosie, dluzszy - w pamieci z malloc(dlugosc + 1). Gdy malloc() odmowi,
// linia PRZEPADA bez sladu. Odmowic moze scenariusz (owijka malloc() w
// symulator_panelu.cpp) albo wprost g_atrapa.serialBezPamieci.
size_t AtrapaSerial::printf(const char* fmt, ...) {
  char bufor[1024];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(bufor, sizeof(bufor), fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  if (n >= 64) {
    void* p = g_atrapa.serialBezPamieci ? nullptr : malloc((size_t)n + 1);
    if (p == nullptr) {
      g_atrapa.serialLiniiUtraconych++;
      {
        AtrapaBezPomiaru bezPomiaru;
        g_atrapa.serialOstatniaUtracona.assign(bufor, 60);
      }
      return 0;
    }
    free(p);
  }
  dopisz(bufor);
  return (size_t)n;
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
// begin() BEZ podanego BSSID - wybor AP robi "sterownik WiFi":
//   WIFI_FAST_SCAN (domyslne w arduino-esp32): pierwsza siec o pasujacej
//     nazwie, kanaly przeszukiwane od najnizszego (ESP-IDF: "scan will end
//     after find SSID match AP");
//   WIFI_ALL_CHANNEL_SCAN + WIFI_CONNECT_AP_BY_SIGNAL: najsilniejsza.
wl_status_t AtrapaWiFi::begin(const char* ssid, const char*, int32_t, const uint8_t* bssid, bool) {
  g_atrapa.liczbaBegin++;
  if (bssid != nullptr) {
    g_atrapa.liczbaBeginZBssid++;
    if (g_atrapa.metodaSkanu == (int)WIFI_ALL_CHANNEL_SCAN) g_atrapa.liczbaBeginZBssidPelnySkan++;
  }
  g_atrapa.celUstawiony = false;
  const AtrapaAp* wybrany = nullptr;
  for (const AtrapaAp& ap : g_atrapa.eter) {
    if (ap.ssid != (ssid ? ssid : "")) continue;
    if (bssid != nullptr) {
      // Z podanym BSSID radio laczy sie WYLACZNIE z tym nadajnikiem. Kanal
      // jest tylko podpowiedzia, od ktorego zaczac szukanie (ESP-IDF:
      // "scan starting from the specified channel"), wiec go nie sprawdzamy.
      if (memcmp(ap.bssid, bssid, 6) == 0) { wybrany = &ap; break; }
      continue;
    }
    if (wybrany == nullptr) { wybrany = &ap; continue; }
    if (g_atrapa.metodaSkanu == (int)WIFI_FAST_SCAN) {
      if (ap.kanal < wybrany->kanal) wybrany = &ap;
    } else {
      if (ap.rssi > wybrany->rssi) wybrany = &ap;
    }
  }
  if (wybrany != nullptr) {
    g_atrapa.celUstawiony = true;
    memcpy(g_atrapa.celBssid, wybrany->bssid, 6);
    g_atrapa.celWcielenie = wybrany->wcielenie;
    g_atrapa.polaczenieOdMs = g_atrapa.terazMs + g_atrapa.czasLaczeniaMs;
  }
  return status();
}

bool AtrapaWiFi::disconnect(bool, bool) {
  g_atrapa.liczbaDisconnect++;
  const bool bylPolaczony = (g_atrapa.polaczonyAp() != nullptr);
  g_atrapa.celUstawiony = false;
  if (bylPolaczony) g_atrapa.zglosRozlaczenie(8);   // WIFI_REASON_ASSOC_LEAVE - rozlaczenie na wlasne zyczenie
  return true;
}

// WiFi.onEvent(): atrapa zna tylko zdarzenie rozlaczenia stacji.
static AtrapaWiFi::FunkcjaZdarzenia g_funkcjaZdarzenia = nullptr;
static void atrapaPrzekazRozlaczenie(int kod) {
  if (g_funkcjaZdarzenia == nullptr) return;
  arduino_event_info_t info;
  memset(&info, 0, sizeof(info));
  info.wifi_sta_disconnected.reason = (uint8_t)kod;
  g_funkcjaZdarzenia(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, info);
}
int AtrapaWiFi::onEvent(FunkcjaZdarzenia f) {
  g_funkcjaZdarzenia = f;
  g_atrapa.naRozlaczenie = atrapaPrzekazRozlaczenie;
  return 1;
}

wl_status_t AtrapaWiFi::status() { return g_atrapa.polaczonyAp() != nullptr ? WL_CONNECTED : WL_DISCONNECTED; }

int16_t AtrapaWiFi::scanNetworks(bool async, bool, bool, uint32_t, uint8_t, const char*, const uint8_t*) {
  g_atrapa.liczbaSkanow++;
  g_atrapa.wynikSkanu.clear();
  g_atrapa.skanTrwa = true;
  g_atrapa.skanKoniecMs = g_atrapa.terazMs + g_atrapa.czasSkanuMs;
  if (async) return WIFI_SCAN_RUNNING;
  // Wersja blokujaca: rdzen czeka w srodku (bez karmienia watchdoga zadania).
  while (scanComplete() == WIFI_SCAN_RUNNING) {
    if (g_atrapa.skanZawieszony && g_atrapa.terazMs - (g_atrapa.skanKoniecMs - g_atrapa.czasSkanuMs) > 60000UL) return WIFI_SCAN_FAILED;
    atrapa_uplyw_czasu(10);
  }
  return scanComplete();
}

int16_t AtrapaWiFi::scanComplete() {
  AtrapaBezPomiaru bezPomiaru;
  if (g_atrapa.skanTrwa) {
    if (g_atrapa.skanZawieszony || g_atrapa.terazMs < g_atrapa.skanKoniecMs) return WIFI_SCAN_RUNNING;
    g_atrapa.skanTrwa = false;
    g_atrapa.wynikSkanu = g_atrapa.eter;
    // ESP-IDF oddaje liste posortowana od najsilniejszego sygnalu.
    std::sort(g_atrapa.wynikSkanu.begin(), g_atrapa.wynikSkanu.end(),
              [](const AtrapaAp& a, const AtrapaAp& b) { return a.rssi > b.rssi; });
  }
  return (int16_t)g_atrapa.wynikSkanu.size();
}

void AtrapaWiFi::scanDelete() { AtrapaBezPomiaru bezPomiaru; g_atrapa.wynikSkanu.clear(); }

// Jak STAClass::BSSID() w arduino-esp32 3.x: bez polaczenia zwraca NULL
// (chyba ze wolajacy podal wlasny bufor - wtedy same zera).
uint8_t* AtrapaWiFi::BSSID(uint8_t* bufor) {
  static uint8_t wynik[6];
  const AtrapaAp* ap = g_atrapa.polaczonyAp();
  if (bufor != nullptr) {
    if (ap == nullptr) memset(bufor, 0, 6);
    else memcpy(bufor, ap->bssid, 6);
    return bufor;
  }
  if (ap == nullptr) return nullptr;
  memcpy(wynik, ap->bssid, 6);
  return wynik;
}
int32_t AtrapaWiFi::channel() { const AtrapaAp* ap = g_atrapa.polaczonyAp(); return ap ? ap->kanal : 0; }
int8_t AtrapaWiFi::RSSI() { const AtrapaAp* ap = g_atrapa.polaczonyAp(); return ap ? (int8_t)ap->rssi : 0; }
String AtrapaWiFi::SSID() const { const AtrapaAp* ap = g_atrapa.polaczonyAp(); return ap ? String(ap->ssid.c_str()) : String(); }

String AtrapaWiFi::SSID(uint8_t i) { return i < g_atrapa.wynikSkanu.size() ? String(g_atrapa.wynikSkanu[i].ssid.c_str()) : String(); }
uint8_t* AtrapaWiFi::BSSID(uint8_t i, uint8_t* bufor) {
  if (i >= g_atrapa.wynikSkanu.size()) return nullptr;
  if (bufor != nullptr) { memcpy(bufor, g_atrapa.wynikSkanu[i].bssid, 6); return bufor; }
  return g_atrapa.wynikSkanu[i].bssid;
}
int32_t AtrapaWiFi::channel(uint8_t i) { return i < g_atrapa.wynikSkanu.size() ? g_atrapa.wynikSkanu[i].kanal : 0; }
int32_t AtrapaWiFi::RSSI(uint8_t i) { return i < g_atrapa.wynikSkanu.size() ? g_atrapa.wynikSkanu[i].rssi : 0; }
