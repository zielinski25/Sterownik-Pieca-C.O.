// Atrapa ArduinoOTA.h (arduino-esp32 3.x) - wgrywanie programu z PlatformIO
// przez WiFi (espota, port UDP/TCP 3232). Firmware (v1.5.4) tylko ustawia
// nazwe, haslo i cztery wywolania zwrotne, raz wola begin() i potem handle()
// w kazdym obrocie petli. Tutaj: begin()/handle() liczone w g_atrapa, a
// scenariusz sam "wgrywa" plik przez atrapaOtaVscodeWgraj() - wywoluje te
// same zwrotne co biblioteka w _runUpdate(): onStart, onProgress (co kawalek),
// onEnd, a na koncu ESP.restart() (tak robi ArduinoOTA przy _rebootOnSuccess);
// przy bledzie onError i bez restartu. Samego gniazda, uwierzytelnienia
// (MD5 hasla) ani Update.begin()/write() atrapa nie modeluje.
#pragma once
#include <functional>
#include "Arduino.h"
#include "atrapa.h"

typedef enum { OTA_AUTH_ERROR, OTA_BEGIN_ERROR, OTA_CONNECT_ERROR, OTA_RECEIVE_ERROR, OTA_END_ERROR } ota_error_t;
#ifndef U_FLASH
#define U_FLASH 0
#endif
#ifndef U_SPIFFS
#define U_SPIFFS 100
#endif

class ArduinoOTAClass {
 public:
  typedef std::function<void(void)> THandlerFunction;
  typedef std::function<void(ota_error_t)> THandlerFunction_Error;
  typedef std::function<void(unsigned int, unsigned int)> THandlerFunction_Progress;

  ArduinoOTAClass& setHostname(const char* n) { hostname = n ? n : ""; return *this; }
  ArduinoOTAClass& setPassword(const char* h) { haslo = h ? h : ""; return *this; }
  ArduinoOTAClass& setPort(uint16_t p) { port = p; return *this; }
  ArduinoOTAClass& setMdnsEnabled(bool w) { mdns = w; return *this; }
  ArduinoOTAClass& setRebootOnSuccess(bool w) { restartPoSukcesie = w; return *this; }
  ArduinoOTAClass& onStart(THandlerFunction f) { naStart = f; return *this; }
  ArduinoOTAClass& onEnd(THandlerFunction f) { naKoniec = f; return *this; }
  ArduinoOTAClass& onError(THandlerFunction_Error f) { naBlad = f; return *this; }
  ArduinoOTAClass& onProgress(THandlerFunction_Progress f) { naPostep = f; return *this; }
  void begin() { g_atrapa.otaVscodeBegin++; gotowe = true; }
  void handle() { if (gotowe) g_atrapa.otaVscodeHandle++; }
  void end() { gotowe = false; }
  int getCommand() { return U_FLASH; }
  String getHostname() { return hostname; }

  // Dla scenariuszy: udawane wgranie `bajtow` bajtow w kawalkach po 1460 B
  // (jeden segment TCP), kazdy kawalek to g_atrapa.otaVscodeMsNaKawalek czasu.
  // blad < 0 = powodzenie (konczy sie ESP.restart()), inaczej kod ota_error_t.
  void symulujWgranie(unsigned int bajtow, int blad = -1) {
    if (!gotowe) return;
    if (naStart) naStart();
    unsigned int zrobione = 0;
    while (zrobione < bajtow) {
      zrobione += 1460;
      if (zrobione > bajtow) zrobione = bajtow;
      atrapa_uplyw_czasu(g_atrapa.otaVscodeMsNaKawalek);
      if (naPostep) naPostep(zrobione, bajtow);
      if (blad >= 0 && zrobione * 2 >= bajtow) {   // blad w polowie
        if (naBlad) naBlad((ota_error_t)blad);
        return;
      }
    }
    if (naKoniec) naKoniec();
    if (restartPoSukcesie) ESP.restart();
  }

  String hostname, haslo;
  uint16_t port = 3232;
  bool mdns = true, restartPoSukcesie = true, gotowe = false;
  THandlerFunction naStart, naKoniec;
  THandlerFunction_Error naBlad;
  THandlerFunction_Progress naPostep;
};

inline ArduinoOTAClass ArduinoOTA;
