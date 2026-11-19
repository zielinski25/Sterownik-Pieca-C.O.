// Atrapa WiFi.h (arduino-esp32 3.x) - model radia opisany w atrapa.h.
#pragma once
#include "Arduino.h"

typedef enum {
  WL_NO_SHIELD = 255,
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_SCAN_COMPLETED = 2,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6
} wl_status_t;

typedef enum { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 } wifi_mode_t;
// [v1.4.7] wifi_power_t jak w WiFiGeneric.h rdzenia 3.x - wartosci to cwiartki dBm.
typedef enum {
  WIFI_POWER_19_5dBm = 78, WIFI_POWER_19dBm = 76, WIFI_POWER_18_5dBm = 74, WIFI_POWER_17dBm = 68,
  WIFI_POWER_15dBm = 60, WIFI_POWER_13dBm = 52, WIFI_POWER_11dBm = 44, WIFI_POWER_8_5dBm = 34,
  WIFI_POWER_7dBm = 28, WIFI_POWER_5dBm = 20, WIFI_POWER_2dBm = 8, WIFI_POWER_MINUS_1dBm = -4
} wifi_power_t;
typedef enum { WIFI_FAST_SCAN = 0, WIFI_ALL_CHANNEL_SCAN } wifi_scan_method_t;
typedef enum { WIFI_CONNECT_AP_BY_SIGNAL = 0, WIFI_CONNECT_AP_BY_SECURITY } wifi_sort_method_t;

#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)

// Zdarzenia sieci - tylko to, czego uzywa firmware (kod przyczyny rozlaczenia).
// Nazwy typow i pol jak w arduino-esp32 3.x / ESP-IDF.
typedef enum {
  ARDUINO_EVENT_NONE = 0,
  ARDUINO_EVENT_WIFI_STA_CONNECTED,
  ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
  ARDUINO_EVENT_WIFI_STA_GOT_IP,
  ARDUINO_EVENT_MAX
} arduino_event_id_t;
typedef struct {
  uint8_t ssid[32];
  uint8_t ssid_len;
  uint8_t bssid[6];
  uint8_t reason;
  int8_t rssi;
} wifi_event_sta_disconnected_t;
typedef union {
  wifi_event_sta_disconnected_t wifi_sta_disconnected;
} arduino_event_info_t;
typedef arduino_event_id_t WiFiEvent_t;
typedef arduino_event_info_t WiFiEventInfo_t;

class AtrapaWiFi {
 public:
  bool mode(wifi_mode_t m) { g_atrapa.wifiTryb = (int)m; return true; }
  // [v1.4.7] Jak WiFiGenericClass::setTxPower(): esp_wifi_set_max_tx_power()
  // zwraca ESP_ERR_WIFI_NOT_INIT, dopoki WiFi.mode() nie wystartowalo radia.
  bool setTxPower(wifi_power_t moc) {
    if (g_atrapa.wifiTryb == 0) { g_atrapa.txPrzedStartem++; return false; }
    g_atrapa.txMoc = (int)moc; g_atrapa.txUstawien++; return true;
  }
  wifi_power_t getTxPower() { return (wifi_power_t)g_atrapa.txMoc; }
  bool persistent(bool p) { g_atrapa.wifiPersistent = p; return true; }
  bool config(IPAddress, IPAddress, IPAddress) { return true; }
  void setScanMethod(wifi_scan_method_t m) { g_atrapa.metodaSkanu = (int)m; }
  void setSortMethod(wifi_sort_method_t m) { g_atrapa.metodaSortowania = (int)m; }

  // Te same sygnatury co WiFiSTAClass::begin() / WiFiScanClass w rdzeniu 3.x.
  wl_status_t begin(const char* ssid, const char* haslo = nullptr, int32_t kanal = 0, const uint8_t* bssid = nullptr, bool lacz = true);
  bool disconnect(bool wylaczRadio = false, bool wymazAp = false);
  wl_status_t status();
  typedef void (*FunkcjaZdarzenia)(arduino_event_id_t, arduino_event_info_t);
  int onEvent(FunkcjaZdarzenia f);   // w rdzeniu: std::function + opcjonalny numer zdarzenia

  int16_t scanNetworks(bool async = false, bool ukryte = false, bool pasywny = false, uint32_t msNaKanal = 300, uint8_t kanal = 0,
                       const char* ssid = nullptr, const uint8_t* bssid = nullptr);
  int16_t scanComplete();
  void scanDelete();

  // po polaczeniu
  uint8_t* BSSID(uint8_t* bufor = nullptr);
  int32_t channel();
  int8_t RSSI();
  String SSID() const;
  IPAddress localIP() { return IPAddress(192, 168, 4, 2); }

  // wynik skanu
  String SSID(uint8_t i);
  uint8_t* BSSID(uint8_t i, uint8_t* bufor = nullptr);
  int32_t channel(uint8_t i);
  int32_t RSSI(uint8_t i);
};
extern AtrapaWiFi WiFi;

// Klient TCP - uzywany tylko przez ekran Wykresy (HTTPClient) i OTA; w
// testach te sciezki nie sa uruchamiane, wiec wystarczy "pusty" strumien.
class WiFiClient {
 public:
  int available() { return 0; }
  int read() { return -1; }
  int read(uint8_t*, size_t) { return 0; }
  size_t readBytes(char*, size_t) { return 0; }
  size_t readBytes(uint8_t*, size_t) { return 0; }
  size_t readBytesUntil(char, char*, size_t) { return 0; }
  bool connected() { return false; }
  void stop() {}
  void setTimeout(uint32_t) {}
};
