// Atrapa HTTPUpdate.h - OTA panelu. update() niczego nie pobiera: odnotowuje
// adres, "trwa" g_atrapa.otaTrwaMs i zwraca g_atrapa.otaWynik (domyslnie
// "nie udalo sie"). Opis pol: mock/atrapa.h, sekcja "aktualizacja OTA panelu".
#pragma once
#include <functional>
#include "Arduino.h"
#include "WiFi.h"

typedef enum { HTTP_UPDATE_FAILED, HTTP_UPDATE_NO_UPDATES, HTTP_UPDATE_OK } t_httpUpdate_return;

class AtrapaHttpUpdate {
 public:
  void rebootOnUpdate(bool restartSam) { g_atrapa.otaRestartSam = restartSam ? 1 : 0; }
  void setLedPin(int, uint8_t = 0) {}
  void onStart(std::function<void()>) {}
  void onEnd(std::function<void()>) {}
  void onProgress(std::function<void(int, int)>) {}
  void onError(std::function<void(int)>) {}
  t_httpUpdate_return update(WiFiClient&, const String& adres, const String& = String()) {
    g_atrapa.otaWywolan++;
    g_atrapa.otaAdres = adres.c_str();
    g_atrapa.otaWdtNadzorowal = g_atrapa.wdtNadzoruje;
    // Pobranie i zapis: czas plynie, loop() nie jest wolane. Krokami po 100 ms,
    // zeby model watchdoga (atrapa_uplyw_czasu) widzial caly ten okres.
    for (unsigned long zostalo = g_atrapa.otaTrwaMs; zostalo > 0;) {
      const unsigned long krok = zostalo > 100 ? 100 : zostalo;
      atrapa_uplyw_czasu(krok);
      zostalo -= krok;
    }
    return (t_httpUpdate_return)g_atrapa.otaWynik;
  }
  int getLastError() { return -1; }
  String getLastErrorString() { return String("atrapa"); }
};
extern AtrapaHttpUpdate httpUpdate;
