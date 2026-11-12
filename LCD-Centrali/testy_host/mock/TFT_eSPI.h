// Atrapa TFT_eSPI.h - ekran 480x320 (obrot 1) jako bufor ramki w g_atrapa.ekran.
// Wiernie odtworzone jest to, czego uzywa lvgl_port.cpp (setAddrWindow +
// pushColors) oraz wypelnianie prostokatow; napisy tft.drawString() sa tylko
// ZAPISYWANE do g_atrapa.napisyTft (test sprawdza ich tresc, nie piksele).
#pragma once
#include "Arduino.h"

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define TFT_RED 0xF800
#define TFT_GREEN 0x07E0
#define TFT_BLUE 0x001F
#define TFT_MAGENTA 0xF81F
#define TFT_YELLOW 0xFFE0

#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define CL_DATUM 3
#define MC_DATUM 4
#define CC_DATUM 4
#define MR_DATUM 5
#define CR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

class TFT_eSPI {
 public:
  void init() {}
  void begin() {}
  void setRotation(uint8_t) {}
  int16_t width() { return 480; }
  int16_t height() { return 320; }
  void writecommand(uint8_t) {}
  void writedata(uint8_t) {}
  void startWrite() {}
  void endWrite() {}

  void fillScreen(uint32_t kolor) { fillRect(0, 0, 480, 320, kolor); }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t kolor) {
    for (int32_t yy = y; yy < y + h; yy++)
      for (int32_t xx = x; xx < x + w; xx++)
        if (xx >= 0 && xx < 480 && yy >= 0 && yy < 320) g_atrapa.ekran[yy * 480 + xx] = (uint16_t)kolor;
  }
  void drawRect(int32_t, int32_t, int32_t, int32_t, uint32_t) {}
  void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t, uint32_t kolor) { fillRect(x, y, w, h, kolor); }
  void drawRoundRect(int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) {}
  void drawCircle(int32_t, int32_t, int32_t, uint32_t) {}
  void fillCircle(int32_t, int32_t, int32_t, uint32_t) {}
  void drawLine(int32_t, int32_t, int32_t, int32_t, uint32_t) {}
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t kolor) { fillRect(x, y, w, 1, kolor); }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t kolor) { fillRect(x, y, 1, h, kolor); }
  void drawPixel(int32_t x, int32_t y, uint32_t kolor) { fillRect(x, y, 1, 1, kolor); }
  void fillTriangle(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) {}
  void drawTriangle(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) {}
  void drawBitmap(int16_t, int16_t, const uint8_t*, int16_t, int16_t, uint16_t) {}
  void drawBitmap(int16_t, int16_t, const uint8_t*, int16_t, int16_t, uint16_t, uint16_t) {}
  void drawXBitmap(int16_t, int16_t, const uint8_t*, int16_t, int16_t, uint16_t) {}
  void fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t, uint32_t kolor, uint32_t = 0x00FFFFFF) { fillRect(x, y, w, h, kolor); }
  void drawSmoothRoundRect(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t = 0x00FFFFFF, uint8_t = 0xF) {}

  void setTextDatum(uint8_t) {}
  void setTextColor(uint16_t) {}
  void setTextColor(uint16_t, uint16_t, bool = false) {}
  void setTextFont(uint8_t) {}
  void setTextSize(uint8_t) {}
  void setTextPadding(uint16_t) {}
  void setFreeFont(const void*) {}
  int16_t textWidth(const char* s, uint8_t = 1) { return (int16_t)(8 * strlen(s)); }
  int16_t textWidth(const String& s, uint8_t = 1) { return (int16_t)(8 * s.length()); }
  int16_t fontHeight(int16_t = 1) { return 16; }
  int16_t drawString(const char* s, int32_t, int32_t, uint8_t = 1) { g_atrapa.napisyTft.push_back(s ? s : ""); return 0; }
  int16_t drawString(const String& s, int32_t x, int32_t y, uint8_t f = 1) { return drawString(s.c_str(), x, y, f); }
  int16_t drawNumber(long, int32_t, int32_t, uint8_t = 1) { return 0; }
  int16_t drawCentreString(const char*, int32_t, int32_t, uint8_t) { return 0; }

  // Sciezka uzywana przez LVGL (lvgl_port.cpp -> lvgl_disp_flush()).
  void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h) { ox_ = x; oy_ = y; ow_ = w; oh_ = h; pozycja_ = 0; }
  void pushColors(uint16_t* dane, uint32_t ile, bool = true) {
    for (uint32_t i = 0; i < ile; i++, pozycja_++) {
      if (ow_ <= 0) break;
      int32_t xx = ox_ + (int32_t)(pozycja_ % (uint32_t)ow_);
      int32_t yy = oy_ + (int32_t)(pozycja_ / (uint32_t)ow_);
      if (xx >= 0 && xx < 480 && yy >= 0 && yy < 320 && yy < oy_ + oh_) g_atrapa.ekran[yy * 480 + xx] = dane[i];
    }
  }
  void pushColor(uint16_t kolor) { pushColors(&kolor, 1); }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }

 private:
  int32_t ox_ = 0, oy_ = 0, ow_ = 0, oh_ = 0;
  uint32_t pozycja_ = 0;
};

// Sprite - uzywany tylko przez stary dashboard TFT (LVGL_POC_TEST=0).
class TFT_eSprite : public TFT_eSPI {
 public:
  explicit TFT_eSprite(TFT_eSPI*) {}
  void* createSprite(int16_t, int16_t) { return this; }
  void deleteSprite() {}
  void fillSprite(uint32_t) {}
  void setColorDepth(int8_t) {}
  void pushSprite(int32_t, int32_t) {}
  void pushSprite(int32_t, int32_t, uint16_t) {}
};
