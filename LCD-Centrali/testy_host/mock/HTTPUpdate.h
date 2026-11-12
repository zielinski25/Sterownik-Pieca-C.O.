// Atrapa HTTPUpdate.h - OTA panelu. W testach centrala nigdy nie zglasza
// aktualizacji, wiec update() nie jest wolane; gdyby bylo - "nie udalo sie".
#pragma once
#include <functional>
#include "Arduino.h"
#include "WiFi.h"

typedef enum { HTTP_UPDATE_FAILED, HTTP_UPDATE_NO_UPDATES, HTTP_UPDATE_OK } t_httpUpdate_return;

class AtrapaHttpUpdate {
 public:
  void rebootOnUpdate(bool) {}
  void setLedPin(int, uint8_t = 0) {}
  void onStart(std::function<void()>) {}
  void onEnd(std::function<void()>) {}
  void onProgress(std::function<void(int, int)>) {}
  void onError(std::function<void(int)>) {}
  t_httpUpdate_return update(WiFiClient&, const String&, const String& = String()) { return HTTP_UPDATE_FAILED; }
  int getLastError() { return -1; }
  String getLastErrorString() { return String("atrapa"); }
};
extern AtrapaHttpUpdate httpUpdate;
